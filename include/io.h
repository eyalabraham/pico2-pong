/* io.h
 *
 * IO initialization
 *
 */

#ifndef     __IO_H__
#define     __IO_H__

#include    <stdint.h>

/* ----------------------------------------------------------------------------
 * Module definitions
 */
#define     GPIO_HSTX_BASE              12
#define     GPIO_SYNC                   GPIO_HSTX_BASE
#define     GPIO_PIXEL                  (GPIO_HSTX_BASE+1)
#define     GPIO_TIMING_PIN             2

#define     PWM_OUTPUT_GPIO             4
 
#define     CLOCK_GPOUT0                21
#define     CLOCK_GPIN0                 20

#define     DMA_CHAN_NUM                0

/* Timing conatants for 30Hz frame rate
 */
#define     TIME_100MSEC                3
#define     TIME_1SEC                   30
#define     TIME_10SEC                  300

/* Module functions
 */
void        io_init(void);
uint16_t    io_adc_read(void);
void        io_sound_on(uint16_t pitch);
void        io_sound_off(void);
void        io_timing_pin(int state);
int         io_is_vert_retrace(void);

#endif  /* __IO_H__ */