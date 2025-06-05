/* pico-pong.c
 *
 * Video generator and "pong" game for RPi Pico 2
 * For NTSC video input
 *
 */

#include    <stdio.h>

#include    "io.h"
#include    "video.h"
#include    "ponggame.h"

/* ----------------------------------------------------------------------------
 * Global definitions
 */
#define     VERSION     "v1.0"
/* ----------------------------------------------------------------------------
 * Function prototypes
 */

/* ----------------------------------------------------------------------------
 * Global variables
 */
int     game_cycle_run = 0;

/***************************************************************
 * main()
 * 
 */
int main()
{
    io_init();
    video_init();
    ponggame_init();

    printf("---- Starting -----\n");
    printf("pico-pong %s %s %s\n", VERSION, __DATE__, __TIME__);

    while (1)
    {
        // gpio_put(PICO_DEFAULT_LED_PIN, 0);
        // sleep_ms(1000);
        // gpio_put(PICO_DEFAULT_LED_PIN, 1);
        // sleep_ms(50);

        if ( io_is_vert_retrace() )
        {
            if ( game_cycle_run )
            {
                ponggame();
                game_cycle_run = 0;
            }
        }
        else
        {
            game_cycle_run = 1;
        }
    }
}
