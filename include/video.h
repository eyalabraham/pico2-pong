/* video.h
 *
 * Video scan line and sync
 *
 */

#ifndef     __VIDEO_H__
#define     __VIDEO_H__

#include    <stdint.h>

typedef enum
{
    CLEAR,
    SET,
    FLIP
} pixel_action_t;

typedef struct
{
    uint8_t    *bitmap;     // bitmap byte-array
    uint32_t    col_count;  // in pixels, non-zerp
    uint32_t    row_count;  // in pixels, non-zero
} bit_blit_t;


/* Module functions
 */
void        video_init(void);

void        video_clear_screen(int color);
void        video_set_default_action(pixel_action_t action);
void        video_set_pixel(uint32_t x, uint32_t y);
void        video_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void        video_box(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void        video_circle(uint32_t x0, uint32_t y0, uint32_t r);
void        video_flood_fill(uint32_t x0, uint32_t y0);
void        video_bit_blit(uint32_t x0, uint32_t y0, bit_blit_t *bitmap);
void        video_write_text(uint32_t x, uint32_t y, char *text);

uint32_t*   video_get_even_field(void);
uint32_t*   video_get_odd_field(void);

uint32_t    video_get_x_res(void);
uint32_t    video_get_y_res(void);

#endif  /* __VIDEO_H_ */