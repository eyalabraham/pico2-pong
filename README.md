# PONG game for Raspberry Pi Pico 2

Video generator and "pong" game for RPi Pico 2 for NTSC composite video input. The project was created as an exercise in using Raspberry Pi Pico 0 peripherals.

## Resources

- NTSC video timing https://www.batsocks.co.uk/readme/video_timing.htm
- NTSC/PAL video format https://martin.hinner.info/vga/pal.html
- Bitmap drawing https://www.pixilart.com/

## Picture scan lines

```
PRE_EQUALIZING_PULSES       scan line    0 ... 2
VERTICAL_SYNC               scan line    3 ... 5
POST_EQUALIZING_PULSES      scan line    6 ... 8
PRE_RENDER_BLANK_SCAN_LINE  scan line    9 ... 29
FIRST_ACTIVE_SCAN_LINE      scan line   30 ... 245
POST_RENDER_BLANK_SCAN_LINE scan line  246 ... 262

```

Accounting for overscan, the resolution of the visible pixels is 576 pixel wide (reduced by overscan out of 640) by 432 pixels tall (reduced by overscan out of 480).

## Video generation

HSTX and DMA used to generate video pixel and sync signals. DMA moved 32bit words to HSTX that is configured to shift 16 bit pairs (pixel, sync) to HSTX GPIO pins. HSTX is clocked by 12MHz derived fom the USB PLL source divided by 4. The USB PLL divided source is routed to clock output GPOUT0 that is tied to GPIN0. GPIN0 is the auxiliary clock source for HSTX.

The `io.c` module handles low level IO configuration and access, the `video.c` module provides a set of video utility functions for drawing and text output, and the `ponggame.c` contains game code.

## Audio beeps

TBD

## GPIO pin assignments

```
                            350
(HSTX1) GP13 pin-17 o-----/\/\/\/-----+             Pixel video
                                      |
                            1K        |
(HSTX0) GP12 pin-16 o--+--/\/\/\/-----+-----------o SYNC + Pixel video signal to monitor/TV
                       |
                       +--------------------------o SYNC for scope external trigger
  
(ADC0) pin-31       o-----------------------------o 'paddle' center tap
    
(PWM CH2A) pin-6    o-----------------------------o Audio out
```

