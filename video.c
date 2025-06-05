/* video.c
 *
 * Video rendering functions
 *
 */

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "scanline.h"
#include    "video.h"
#include    "io.h"

/* Active pixels start at the 7th DWORD.
 * 16 MSBs in each DWORD, 40 DWORDs in a line.
 */
static int              initialized = 0;
static pixel_action_t   pixel_action = SET;

static uint32_t video_buffer[VIDEO_Y_RESOLUTION][SCAN_LINE_BUF_LEN];

/***************************************************************
 * video_get_even_field()
 * 
 *  Return pointer to first even scan line
 * 
 *  Param:  none
 *  return: Scan line pointer
 * 
 */
uint32_t* video_get_even_field(void)
{
    return  &video_buffer[1][0];
}

/***************************************************************
 * video_get_odd_field()
 * 
 *  Return pointer to first odd scan line
 * 
 *  Param:  none
 *  return: Scan line pointer
 * 
 */
uint32_t* video_get_odd_field(void)
{
    return  &video_buffer[0][0];
}

/***************************************************************
 * video_init()
 * 
 *  Initialize video buffer
 * 
 *  Param:  none
 *  return: none
 * 
 */
void video_init(void)
{
    int         i;

    memset(video_buffer, 0, sizeof(video_buffer));

    for ( i = 0; i < VIDEO_Y_RESOLUTION; i++ )
    {
        memcpy(&video_buffer[i][0], vid_blank_scan_line, (ACTIVE_VIDEO_OFFSET * sizeof(uint32_t)));
    }

    initialized = 1;
}

/***************************************************************
 * video_clear_screen()
 * 
 *  Clear video screen to color (1-white, 0-black)
 * 
 *  Param:  1-white, 0-black
 *  return: none
 * 
 */
void video_clear_screen(int color)
{
    int         i, j;
    uint32_t    c;

    if ( !initialized )
        return;
    
    c = color ? (0xffff0000) : (0x00000000);

    for ( i = 0; i < VIDEO_Y_RESOLUTION; i++ )
        for ( j = 0; j < 36; j++)
            video_buffer[i][j + ACTIVE_VIDEO_OFFSET] = c;
}

/***************************************************************
 * video_set_default_action()
 * 
 *  Set the default action (SET, RESET, FLIP) for pixel painting.
 * 
 *  Param:  Action
 *  return: none
 * 
 */void video_set_default_action(pixel_action_t action)
{
    pixel_action = action;
}

/***************************************************************
 * video_set_pixel()
 * 
 *  Set pixel value.
 * 
 *  Param:  pixel (x,y) coordinate and color
 *  return: none
 * 
 */
void video_set_pixel(uint32_t x, uint32_t y)
{
    uint32_t    word_index;
    uint32_t    bit_index;

    if ( !initialized )
        return;
    
    if ( x >= VIDEO_X_RESOLUTION  ||
         y >= VIDEO_Y_RESOLUTION )
    {
        return;
    }

    word_index = (x >> 4) + ACTIVE_VIDEO_OFFSET;
    bit_index = 31 - (x & 0x0000000f);

    //printf("(%d, %d) word_index=%d, bit_index=%d\n", x, y, word_index, bit_index);
    
    while ( !io_is_vert_retrace() ) 
    ;

    if ( pixel_action == CLEAR )
        video_buffer[y][word_index] &= 0xfffffffe << bit_index;
    else if ( pixel_action == SET )
        video_buffer[y][word_index] |= 0x00000001 << bit_index;
    else
        video_buffer[y][word_index] ^= 0x00000001 << bit_index;
}

/* ----------------------------------------------------------------------------
 * video_line()
 *
 *  Draw a line in foreground color 'white'
 *  between coordinates (X0,Y0)-(X1,Y1) using Bresenham's line algorithm
 *  Safe to use coordinate outside screen, function will draw clipped lines.
 *
 *  Param:  Line start-end (X0,Y0)-(X1,Y1) coordinates
 *  return: none
 * 
 */
void video_line(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    int     dx, sx;
    int     dy, sy;
    int     err, e2;

    if ( !initialized )
        return;
    
    dx = abs(x1-x0);
    sx = x0<x1 ? 1 : -1;
    dy = abs(y1-y0);
    sy = y0<y1 ? 1 : -1;
    err = (dx>dy ? dx : -dy)/2;

    for(;;)
    {
        video_set_pixel(x0, y0);
        if (x0==x1 && y0==y1) break;
        e2 = err;
        if (e2 >-dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

/* ----------------------------------------------------------------------------
 * video_box()
 *
 *  Draw a line box given corner coordinates.
 *
 *  Param:  Box corner start-end (X0,Y0)-(X1,Y1) coordinates
 *  return: none
 * 
 */
void video_box(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    if ( !initialized )
        return;

    video_line( x0, y0, x1, y0);
    video_line( x1, y0, x1, y1);
    video_line( x1, y1, x0, y1);
    video_line( x0, y1, x0, y0);
}

/* ----------------------------------------------------------------------------
 * video_circle()
 *
 *  Draw a circle given center and radius.
 *
 *  Param:  Circle center and radius
 *  return: none
 * 
 */
void video_circle(uint32_t x0, uint32_t y0, uint32_t r)
{
    if ( !initialized )
        return;
}

/* ----------------------------------------------------------------------------
 * video_flood_fill()
 *
 *  Flood fill a closed shape.
 *
 *  Param:  Starting point inside shape and fill color
 *  return: none
 * 
 */
void  video_flood_fill(uint32_t x0, uint32_t y0)
{
    if ( !initialized )
        return;
}

/* ----------------------------------------------------------------------------
 * video_bit_blit()
 *
 *  Bit blit function places the bit map on top left corner coordinate provided
 *  to the function. The bitmap is painted according to the pixel action.
 *  The function uses the bit_blit_t structure col_count to determine bytes per row.
 *  A new row is started when col_count is reached.
 *
 *  Param:  Starting point o place bit map, bit map parameters, and color.
 *  return: none
 * 
 */
void video_bit_blit(uint32_t x0, uint32_t y0, bit_blit_t *bitmap)
{
    int         full_bytes_in_row;
    int         pixels_in_last_byte;
    int         row;
    int         col;
    int         byte;
    uint8_t    *bitmap_pattern;
    uint8_t     pattern;
    uint8_t     bit_mask;

    if ( !initialized )
        return;

    if ( bitmap->col_count == 0 ||
         bitmap->row_count == 0 )
         return;

    full_bytes_in_row = (bitmap->col_count - 1) >> 3;
    pixels_in_last_byte = bitmap->col_count - (full_bytes_in_row * 8);

    bitmap_pattern = bitmap->bitmap;

    for ( row = 0; row < bitmap->row_count; row++ )
    {
        col = 0;

        for ( byte = 0; byte < full_bytes_in_row; byte++ )
        {
            pattern = *bitmap_pattern++;
            for (int i = 0, bit_mask = 0b10000000; i < 8; i++ )
            {
                if ( bit_mask & pattern )
                    video_set_pixel(x0 + col, y0 + row);
                
                bit_mask >>= 1;
                col++;
            }
        }

        if ( pixels_in_last_byte )
        {
            pattern = *bitmap_pattern++;
            for (int i = 0, bit_mask = 0b10000000; i < pixels_in_last_byte; i++ )
            {
                if ( bit_mask & pattern )
                    video_set_pixel(x0 + col, y0 + row);
                
                bit_mask >>= 1;
                col++;
            }
        }
    }
}

/* ----------------------------------------------------------------------------
 * video_write_text()
 *
 *  Output text to screen starting a point (x,y) that marks
 *  the top left corner of the left most character.
 *  Text does not wrap at screen edge.
 *
 *  Param:  Top left starting point
 *  return: none
 * 
 */
void video_write_text(uint32_t x, uint32_t y, char *text)
{
    if ( !initialized )
        return;
}

/* ----------------------------------------------------------------------------
 * video_get_x_res()
 *
 *  Return X axis resolution in pixels
 *
 *  Param:  none
 *  return: X axis resolution in pixels
 * 
 */
uint32_t video_get_x_res(void)
{
    return (VIDEO_X_RESOLUTION - 1);
}

/* ----------------------------------------------------------------------------
 * video_get_y_res()
 *
 *  Return Y axis resolution in pixels
 *
 *  Param:  none
 *  return: Y axis resolution in pixels
 * 
 */
uint32_t video_get_y_res(void)
{
    return (VIDEO_Y_RESOLUTION - 1);
}