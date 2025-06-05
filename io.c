/* io.c
 *
 * IO initialization
 *
 */

#include    "pico/stdlib.h"

#include    "hardware/gpio.h"
#include    "hardware/adc.h"
#include    "hardware/dma.h"
#include    "hardware/irq.h"
#include    "hardware/clocks.h"
#include    "hardware/pwm.h"
#include    "hardware/structs/hstx_ctrl.h"
#include    "hardware/structs/hstx_fifo.h"
 
#include    "scanline.h"
#include    "video.h"
#include    "io.h"

/* ----------------------------------------------------------------------------
 * Module definitions
 */
/* NTSC Interlace Scan line parameters
 */
#define     LINES_PER_FIELD             262         // 262 and 1/2

#define     PRE_EQUALIZING_PULSES       0           //   0 ... 2
#define     VERTICAL_SYNC               3           //   3 ... 5
#define     POST_EQUALIZING_PULSES      6           //   6 ... 8
#define     PRE_RENDER_BLANK_SCAN_LINE  9           //   9 ... 29
#define     FIRST_ACTIVE_SCAN_LINE      30          //  30 ... 245
#define     POST_RENDER_BLANK_SCAN_LINE 246         // 246 ... 262

#define     LAST_SCAN_LINE              (LINES_PER_FIELD-1)

/* ADC readout averaging parameters.
 * ADC is read at Timer0 rate, with Fclk at 10Mhz the rate
 * is about 38 samples per second. Selecting ADC_AVERAGE_BITS=5
 * will result in averaging 32 samples (approximately 1 second).
 */
#define     ADC_CH0_GPIO        26
#define     ADC_AVERAGE_BITS    3                       // 1, 2, 3, or 4
#define     ADC_AVERAGE_MAX     16
#define     ADC_AVERAGE         (1<<ADC_AVERAGE_BITS)   // Power of 2 (2, 4, 8, 16, 32)
#if (ADC_AVERAGE>ADC_AVERAGE_MAX)
#warning "ADC averaging is out of range. Reduce ADC_AVERAGE_BITS!"
#endif

#define     PWM_DIV             250

/* ----------------------------------------------------------------------------
 * Function prototypes
 */
static void dma_irq_handler();

/* ----------------------------------------------------------------------------
 * Module globals
 */
static volatile int         in_vert_retrace = 0;
static volatile uint32_t    frame_counter = 0;
static int                  pwn_slice_num = -1;

/***************************************************************
 * io_init()
 * 
 *  Initialize IO
 * 
 *  Param:  none
 *  return: none
 * 
 */
void io_init(void)
{
    pwm_config pwm_configuration;

    stdio_init_all();

    /* Turn off LED
     */
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);

    /* Timing pin GPIO output
     */
    gpio_set_function(GPIO_TIMING_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(GPIO_TIMING_PIN, GPIO_OUT);
    gpio_put(GPIO_TIMING_PIN, 0);

     /* ADC
     */
    adc_init();
    adc_gpio_init(ADC_CH0_GPIO);
    adc_select_input(0);
    for ( int i = 0; i < ( 2 << ADC_AVERAGE_BITS); i++ )
    {
        io_adc_read();  // "Prime" the ADC input filter
    }

    /* PWM
     */
    gpio_set_function(PWM_OUTPUT_GPIO, GPIO_FUNC_PWM);
    pwn_slice_num = pwm_gpio_to_slice_num(PWM_OUTPUT_GPIO);
    pwm_set_clkdiv(pwn_slice_num, PWM_DIV);
    pwm_set_clkdiv_mode (pwn_slice_num, PWM_DIV_FREE_RUNNING);

    /* Initalized SYNC and PIXEL GPIOs
     */
    gpio_set_function(GPIO_SYNC, GPIO_FUNC_HSTX);
    gpio_set_function(GPIO_PIXEL, GPIO_FUNC_HSTX);

    /* HSTX clock source and divider
     */

    clock_gpio_init_int_frac8(CLOCK_GPOUT0, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 4, 0);
    clock_configure_gpin(clk_hstx, CLOCK_GPIN0, 1, 1);

    /* Initialize HSTX
     */
    
    hstx_ctrl_hw->bit[GPIO_SYNC - GPIO_HSTX_BASE] =     // GPIO12 = SYNC
        (15u << HSTX_CTRL_BIT0_SEL_P_LSB) |
        (15u << HSTX_CTRL_BIT0_SEL_N_LSB);

    hstx_ctrl_hw->bit[GPIO_PIXEL - GPIO_HSTX_BASE] =    // GPIO13 = PIXEL
        (31u << HSTX_CTRL_BIT1_SEL_P_LSB) |
        (31u << HSTX_CTRL_BIT1_SEL_N_LSB) |
        (HSTX_CTRL_BIT1_INV_BITS);

    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EN_BITS |
        (31u << HSTX_CTRL_CSR_SHIFT_LSB) |              // We have packed 2x 16 bit fields,
        (16u << HSTX_CTRL_CSR_N_SHIFTS_LSB);            // shift left, 1 bit/cycle, 16 times.

    /* Initialize DMA channel (do not enable yet)
     * Set up to transfer a whole pixes + sync buffer to HSTX.
     */
    dma_channel_config c;

    c = dma_channel_get_default_config(DMA_CHAN_NUM);
    channel_config_set_dreq(&c, DREQ_HSTX);
    channel_config_set_read_increment(&c, true);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    dma_channel_configure(
        DMA_CHAN_NUM,
        &c,
        0,
        0,
        0,
        false
    );

    /* Interrupt handler for DMA transfer complete
     */
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_set_irq0_enabled(DMA_CHAN_NUM, true);

    /* Enable DMA transfer
     */
    dma_channel_set_read_addr(DMA_CHAN_NUM, vid_vert_sync, false);
    dma_channel_set_write_addr(DMA_CHAN_NUM, &hstx_fifo_hw->fifo, false);
    dma_channel_set_trans_count(DMA_CHAN_NUM, SCAN_LINE_BUF_LEN, true);
}

/***************************************************************
 * io_adc_read()
 * 
 *  Read paddle ADC
 * 
 *  Param:  none
 *  return: ADC unsigned 16bit value
 * 
 */
uint16_t io_adc_read(void)
{
    uint16_t        adc_readout;

    static uint16_t adc_values[ADC_AVERAGE_MAX] =
            { 0,0,0,0,0,0,0,0,0,0,
              0,0,0,0,0,0 };
    static uint16_t  adc_in = (ADC_AVERAGE - 1);
    static uint16_t  adc_out = 0;
    static uint16_t  adc_sum = 0;

    adc_readout = adc_read();

    adc_sum -= adc_values[adc_out];
    adc_out++;
    adc_out &= (ADC_AVERAGE - 1);

    adc_in++;
    adc_in &= (ADC_AVERAGE - 1);
    adc_values[adc_in] = adc_readout;
    adc_sum += adc_readout;

    return (adc_sum >> ADC_AVERAGE_BITS);
}

/***************************************************************
 * io_sound_on()
 * 
 *  Turn on (PWM) sound generation at a frequency pitch.
 *  Frequency = 600,000 / pitch for 150MHz system clock
 * 
 *  Param:  Frequency pitch constant
 *  return: none
 * 
 */
void io_sound_on(uint16_t pitch)
{
    pwm_set_wrap(pwn_slice_num, pitch);
    pwm_set_chan_level(pwn_slice_num, PWM_CHAN_A, (pitch >> 1));
    pwm_set_enabled(pwn_slice_num, 1);
}

/***************************************************************
 * io_sound_off()
 * 
 *  Turn off (PWM) sound generation
 * 
 *  Param:  none
 *  return: none
 * 
 */
void io_sound_off(void)
{
    pwm_set_enabled(pwn_slice_num, 0);
}

/***************************************************************
 * io_timing_pin()
 * 
 *  Set timing outputpin state
 * 
 *  Param:  Pin state
 *  return: none
 * 
 */
void io_timing_pin(int state)
{
    gpio_put(GPIO_TIMING_PIN, state);
}

/***************************************************************
 * io_is_vert_retrace()
 * 
 *  Return the frame scan phase
 * 
 *  Param:  none
 *  return: True while in vertical sync, false when rendering.
 * 
 */
int io_is_vert_retrace(void)
{
    return in_vert_retrace;
}

/***************************************************************
 * dma_irq_handler()
 * 
 *  Triggered by DMA transfer completion.
 * 
 */
static void dma_irq_handler()
{
    static int          scan_line = 0;
    static int          is_even_field = 1;
    static uint32_t    *scan_line_buffer = vid_blank_scan_line;

    uint32_t            transfer_count = 1;
    
    /* Scan line 0 .. 2
     * Six pre-equalizing pulses
     */
    if ( scan_line < VERTICAL_SYNC )
    {
        /* Will result in a vertical blanking signaled only
         * when an odd field is about to be rendered.
         * Effectively at a frame rate (~30Hz NTSC)
         */
        if ( !is_even_field )
        {
            in_vert_retrace = 1;
        }
        
        scan_line_buffer = vid_equalizing_pulse;
        transfer_count = SCAN_LINE_BUF_LEN;
    }

    /* Scan line 3 .. 5
     * Six seration pulses
     */
    else if ( scan_line < POST_EQUALIZING_PULSES )
    {
        scan_line_buffer = vid_vert_sync;
        transfer_count = SCAN_LINE_BUF_LEN;
    }

    /* Scan line 6 .. 8
     * Six post-equalizing pulses
     */
    else if ( scan_line < PRE_RENDER_BLANK_SCAN_LINE )
    {
        scan_line_buffer = vid_equalizing_pulse;
        transfer_count = SCAN_LINE_BUF_LEN;

        /* Extent last post-equalizing pulse by half scan-line
        * in an even field
        */
        if ( scan_line == (PRE_RENDER_BLANK_SCAN_LINE - 1) && is_even_field )
        {
            transfer_count += 24;
        }
    }

    /* Scan line 9 .. 29
     * Blank video lines
     */
    else if ( scan_line < FIRST_ACTIVE_SCAN_LINE )
    {
        scan_line_buffer = vid_blank_scan_line;
        transfer_count = SCAN_LINE_BUF_LEN;
    }

    /* Scan line 30 .. 245
     * Video lines
     */
    else if ( scan_line == FIRST_ACTIVE_SCAN_LINE )
    {
        in_vert_retrace = 0;

        if ( is_even_field )
            scan_line_buffer = video_get_even_field();
        else
            scan_line_buffer = video_get_odd_field();
        
        transfer_count = SCAN_LINE_BUF_LEN;
    }

    else if ( scan_line < POST_RENDER_BLANK_SCAN_LINE )
    {
        scan_line_buffer += (2 * SCAN_LINE_BUF_LEN);
        transfer_count = SCAN_LINE_BUF_LEN;
    }

    /* Scan line 246 .. 261
     * Blank video lines
     */
    else if ( scan_line < LAST_SCAN_LINE )
    {
        scan_line_buffer = vid_blank_scan_line;
        transfer_count = SCAN_LINE_BUF_LEN;
    }

    /* Insert half blank video line at end of odd field
     */
    else
    {
        if ( !is_even_field )
            {
                scan_line_buffer = vid_blank_half_scan_line;
                transfer_count = HALF_SCAN_LINE_BUF_LEN;
            }
    }

    scan_line++;
    if ( scan_line == LINES_PER_FIELD )
    {
        scan_line = 0;

        if ( is_even_field )
            is_even_field = 0;
        else
            is_even_field = 1;
    }

    dma_channel_acknowledge_irq0(DMA_CHAN_NUM);
    dma_channel_set_read_addr(DMA_CHAN_NUM, scan_line_buffer, false);
    dma_channel_set_trans_count(DMA_CHAN_NUM, transfer_count, true);
}
