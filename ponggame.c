/* ponggame.c
 *
 * pong game module
 *
 */

#include    <stdlib.h>
#include    <stdio.h>

#include    "pico/stdlib.h"

#include    "ponggame.h"
#include    "video.h"
#include    "io.h"
#include    "sprites.h"

/* ----------------------------------------------------------------------------
 * Module definitions
 */
#define     IO_TIMING           0       // Set to '0' or '1' to disable or enable timing GPIO output.

#define     SCREEN_BACKGROUND   0       // Black
#define     BITBLIT_MODE        FLIP
#define     MAX_LIVES           3
#define     MAX_SCORE           99
#define     SCORE_X_POS         300
#define     SCORE_Y_POS         50
#define     LIVES_X_POS         300
#define     LIVES_Y_POS         30
#define     PADDLE_POS_HYST     5
#define     PADDLE_MIN          1526    // Measured
#define     PADDLE_MAX          2500    // Measure

#define     BALL_SPEED          5       // Pixel movement per cycle (frame rate)

#define     SERVE_CYCLE         20      // Counter max value used to "randomize" serve direction
#define     NOSERVE             0       // Serve flag
#define     SERVE               1
#define     UP                  0       // Serve direction
#define     DOWN                1
#define     NONE                0       // Flag score status
#define     LEFT                1
#define     RIGHT               2

#define     SOUNDOFF            0       // Flag which sound to produce
#define     SOUNDACTIVE         1       // Sound is already playing
#define     SOUNDOUT            2
#define     SOUNDPADDLE         3
#define     SOUNDWALL           4
#define     BEEPOUT             3000    // 200Hz  <-- frequency = 600,000 / BEEP*
#define     BEEPPADDLE          400     // 1500Hz
#define     BEEPWALL            300     // 2000Hz
#define     LONGBEEP            (4*TIME_100MSEC)
#define     SHORTBEEP           TIME_100MSEC

/* ----------------------------------------------------------------------------
 * Module function prototypes
 */
static void ponggame_bresenham(void);
static void ponggame_draw_paddle(int x, int y);
static void ponggame_draw_ball(int x, int y);
static void ponggame_draw_score(int score);
static void ponggame_render_score(uint32_t x, uint32_t y, int score);

/* ----------------------------------------------------------------------------
 * Module globals
 */
static bit_blit_t   a_bit_map;
static uint32_t     max_x_res, max_y_res;
static int          score;

/* Paddle
 */
static uint32_t     paddle_x_pos, paddle_y_pos;     // Paddle center!
static uint32_t     ratio;

/* Ball movement
 */
static int          ball_x0, ball_y0;               // Start and end coordinates of ball center movement trajectory line
static int          ball_x1, ball_y1;
static int          dx, sx;                         // Bresenham's line algorithm variables,
static int          dy, sy;                         // These are globals so that ball position is maintained between calls to ponggame()
static int          err, e2;
static int          serve_offset = -SERVE_CYCLE;    // Cycles from 1 to SERVECYCLE and used to pick serve direction (X1,Y1)
static int          serve_dir = UP;                 // Serve direction UP or DOWN
static uint32_t     cycle_count = 0;                // Count game cycles, ~60 cycles per second
static int          serve_flag = SERVE;             // Is it time to serve a new game? 0=no, 1=from-right, 2=from-left
static int          sound_flag = SOUNDOFF;
static int          sound_duration = 0;

/***************************************************************
 * ponggame()
 * 
 *  Pong game module.
 *  Call this module periodically when the display in in vertical blanking state.
 *  Called from pico-pong.c module every other field, at a 30Hz call rate.
 *  Must complete within 1.9mSec, timeing of 30 scan lines of VSYNC + blank overscan.
 * 
 *  Param:  none
 *  return: none
 * 
 */
void  ponggame(void)
{
    static uint32_t     temp_y_paddle;
    int                 pos_diff;
    int                 ball_x0_tmp, ball_y0_tmp;

#if (IO_TIMING==1)
    io_timing_pin(1);
#endif

    ball_x0_tmp = ball_x0;
    ball_y0_tmp = ball_y0;

    /* Place paddle
     */
    temp_y_paddle = (io_adc_read() - PADDLE_MIN) / ratio;

    pos_diff = temp_y_paddle - paddle_y_pos;

    if ( abs(pos_diff) > PADDLE_POS_HYST )
    {
        ponggame_draw_paddle(paddle_x_pos, paddle_y_pos);
        paddle_y_pos = temp_y_paddle;
        ponggame_draw_paddle(paddle_x_pos, paddle_y_pos);
    }

    /* Use this to generate some randomness in ball serving angle
     */
    serve_offset++;
    if (serve_offset > SERVE_CYCLE )
        serve_offset = -SERVE_CYCLE;
    serve_dir = (serve_dir==UP) ? DOWN : UP;

    /* Ball movement and action state machine
     */
    switch ( serve_flag )   // Determine what to do with the next move
    {
    /* No serve, just move the ball and check for
     * collision with wall or paddle
     */
    case NOSERVE:
        /* Check if ball center reached right edge of screen
         * this means that the paddle was missed
         */
        if ( ball_x0 >= max_x_res )
        {
            ponggame_draw_ball(ball_x0, ball_y0); // Clear ball
            score--;
            if ( score < 0 )
                score = 0;
            sound_flag = SOUNDOUT;
            serve_flag = SERVE;
        }

        /* Ball reached the paddle
         */
        else if ( ( ball_x0 + (SPRITE_BALL_COLS / 2)) >= paddle_x_pos &&
                ball_y0 <= (paddle_y_pos + (SPRITE_PADDLE_LENGTH / 2)) &&
                ball_y0 >= (paddle_y_pos - (SPRITE_PADDLE_LENGTH / 2)) )
        {
            ball_x0 -= sx;
            ball_y0 += sy;
            ball_y1 = (sy > 0) ? (max_y_res - 1) : 2;  // Top or bottom of screen
            ball_x1 = ball_x0 + ((-1 * sx * abs(ball_y0-ball_y1) * dx) / dy);
            ponggame_bresenham();
            score++;
            sound_flag = SOUNDPADDLE;
            serve_flag = NOSERVE;
        }

        /* Reached left side wall
         * reverse X trajectory
         */
        else if ( (ball_x0 - (SPRITE_BALL_COLS / 2)) <= (3 * SPRITE_BRICK_COLS) )
        {
            ball_x0 -= sx;
            ball_y0 += sy;
            ball_y1 = (sy > 0) ? (max_y_res - 1) : 2;   // Top or bottom of screen
            ball_x1 = ball_x0 + ((-1 * sx * abs(ball_y0-ball_y1) * dx) / dy);
            ponggame_bresenham();
            sound_flag = SOUNDWALL;
            serve_flag = NOSERVE;
        }

        /* Reached top or bottom of game board
         * reverse Y trajectory
         */
        else if ( ball_y0 <= (SPRITE_BALL_ROWS / 2) ||
                    ball_y0 >= (max_y_res - (SPRITE_BALL_ROWS / 2)) )
        {
            ball_x0 += sx;
            ball_y0 -= sy;
            ball_x1 = (sx > 0) ? paddle_x_pos : (3 * SPRITE_BRICK_COLS);    // *** paddle_x_pos: need to account for X movement of paddle!!
            ball_y1 = ball_y0 + ((-1 * sy * abs(ball_x0-ball_x1) * dy) / dx);
            ponggame_bresenham();
            sound_flag = SOUNDWALL;
            serve_flag = NOSERVE;
        }
        break;

    /* Serve new ball from the right
     */
    case SERVE:
        if ( sound_flag != SOUNDOFF )                       // Wait for 'out' sound to complete
           break;

        ball_x0 = paddle_x_pos - SPRITE_BALL_COLS;          // Serve from center of paddle
        ball_y0 = paddle_y_pos;
        ball_x1 = (max_x_res / 2) + serve_offset;
        ball_y1 = (serve_dir == UP) ? 2 : (max_y_res - 2);  // Top or bottom of screen
        ponggame_bresenham();
        sound_flag = SOUNDPADDLE;
        serve_flag = NOSERVE;

        ball_x0_tmp = ball_x0;
        ball_y0_tmp = ball_y0;
        ponggame_draw_ball(ball_x0, ball_y0);
        break;
    }

    for ( int i = 0; i < BALL_SPEED; i++ )
    {
        e2 = err;                                       // Calculate new ball location
        if (e2 >-dx) { err -= dy; ball_x0 += sx; }
        if (e2 < dy) { err += dx; ball_y0 += sy; }
    }

    if ( serve_flag == NOSERVE )
    {
        ponggame_draw_ball(ball_x0_tmp, ball_y0_tmp);   // Clear current ball location
        ponggame_draw_ball(ball_x0, ball_y0);           // Put ball in new location
    }

    /* Update score
     */
    ponggame_draw_score(score);

    /* Generate sound
     */
    switch ( sound_flag )
    {
    case SOUNDOFF:
        io_sound_off();
        break;

    case SOUNDACTIVE:
        sound_duration--;
        if (sound_duration == 0 )
            sound_flag = SOUNDOFF;
        break;

    case SOUNDPADDLE:
        sound_duration = SHORTBEEP;
        io_sound_on(BEEPPADDLE);
        sound_flag = SOUNDACTIVE;
        break;

    case SOUNDWALL:
        sound_duration = SHORTBEEP;
        io_sound_on(BEEPWALL);
        sound_flag = SOUNDACTIVE;
        break;

    case SOUNDOUT:
        sound_duration = LONGBEEP;
        io_sound_on(BEEPOUT);
        sound_flag = SOUNDACTIVE;
        break;
    }

    cycle_count++;

#if (IO_TIMING==1)
    sleep_us(20);
    io_timing_pin(0);
#endif
}

/* ----------------------------------------------------------------------------
 * ponggame_bresenman()
 *
 *  Calculate ball line trajectory using Bresenham's algorithm
 *
 *  Param:  none
 *  return: none
 * 
 */
static void ponggame_bresenham(void)
{
    dx = abs(ball_x1-ball_x0);
    sx = ball_x0<ball_x1 ? 1 : -1;
    dy = abs(ball_y1-ball_y0);
    sy = ball_y0<ball_y1 ? 1 : -1;
    err = (dx>dy ? dx : -dy)/2;
}

/* ----------------------------------------------------------------------------
 * ponggame_init()
 *
 *  Initialize the game and draw game board
 *
 *  Param:  none
 *  return: none
 * 
 */
void ponggame_init(void)
{
    /* Game variables
     */
    max_x_res = video_get_x_res();
    max_y_res = video_get_y_res();
    score = 0;
    paddle_x_pos = max_x_res - SPRITE_PADDLE_COLS;
    paddle_y_pos = max_y_res / 2;
    ratio = (PADDLE_MAX - PADDLE_MIN) / max_y_res;

    /* Draw game board
     */
    video_clear_screen(SCREEN_BACKGROUND);
    video_set_default_action(BITBLIT_MODE);

    a_bit_map.col_count = SPRITE_HALF_BRICK_COLS;
    a_bit_map.row_count = SPRITE_HALF_BRICK_ROWS;
    a_bit_map.bitmap = &sprite_brick[SPRITE_BRICK_ROWS];

    video_bit_blit(0, 0, &a_bit_map);
    video_bit_blit(2 * SPRITE_BRICK_COLS, 0, &a_bit_map);

    a_bit_map.bitmap = sprite_brick;
    
    video_bit_blit(SPRITE_BRICK_COLS, 13 * SPRITE_BRICK_ROWS, &a_bit_map);

    a_bit_map.col_count = SPRITE_BRICK_COLS;
    a_bit_map.row_count = SPRITE_BRICK_ROWS;

    for ( int i = 0; i < 13; i++ )
    {
        video_bit_blit(0, SPRITE_HALF_BRICK_ROWS + (i * SPRITE_BRICK_ROWS), &a_bit_map);
        video_bit_blit(SPRITE_BRICK_COLS, i * SPRITE_BRICK_ROWS, &a_bit_map);
        video_bit_blit(2 * SPRITE_BRICK_COLS, SPRITE_HALF_BRICK_ROWS + (i * SPRITE_BRICK_ROWS), &a_bit_map);
    }

    video_line(3 * SPRITE_BRICK_COLS, 0, max_x_res, 0);
    video_line(3 * SPRITE_BRICK_COLS, max_y_res, max_x_res, max_y_res);

    a_bit_map.col_count = SPRITE_NUMBERS_COLS;
    a_bit_map.row_count = SPRITE_NUMBERS_ROWS;
    a_bit_map.bitmap = sprite_numbers;
    video_bit_blit(SCORE_X_POS, SCORE_Y_POS, &a_bit_map);

    ponggame_draw_paddle(paddle_x_pos, paddle_y_pos);
    ponggame_draw_score(score);
}

/* ----------------------------------------------------------------------------
 * ponggame_draw_paddle()
 *
 *  Draw paddle on game board given paddle center point
 *
 *  Param:  Paddle center point coordinates
 *  return: none
 * 
 */
static void ponggame_draw_paddle(int x, int y)
{
    if ( y < (SPRITE_PADDLE_LENGTH / 2) )
        y = SPRITE_PADDLE_LENGTH / 2;
    else if ( y > (max_y_res - SPRITE_PADDLE_LENGTH) )
        y = max_y_res - SPRITE_PADDLE_LENGTH + 8;

    a_bit_map.col_count = SPRITE_PADDLE_COLS;
    a_bit_map.row_count = SPRITE_PADDLE_ROWS;
    a_bit_map.bitmap = sprite_paddle;

    video_bit_blit((x - SPRITE_PADDLE_CENTER), (y - SPRITE_PADDLE_LENGTH / 2), &a_bit_map);
}

/* ----------------------------------------------------------------------------
 * ponggame_draw_ball()
 *
 *  Draw ball on game board given ball center point
 *
 *  Param:  Ball center point coordinates
 *  return: none
 * 
 */
static void ponggame_draw_ball(int x, int y)
{
    if ( x < (SPRITE_BALL_COLS / 2) && y < (SPRITE_BALL_ROWS / 2))
        return;

    a_bit_map.col_count = SPRITE_BALL_COLS;
    a_bit_map.row_count = SPRITE_BALL_ROWS;
    a_bit_map.bitmap = sprite_ball;

    video_bit_blit((x - (SPRITE_BALL_COLS / 2)), (y - (SPRITE_BALL_ROWS / 2)), &a_bit_map);
}

/* ----------------------------------------------------------------------------
 * ponggame_draw_score()
 *
 *  Draw score on game board
 *
 *  Param:  Score
 *  return: none
 * 
 */
static void ponggame_draw_score(int score)
{
    static int  previous_score = 0;

    if ( score == previous_score )
        return;

    ponggame_render_score(SCORE_X_POS, SCORE_Y_POS, previous_score);
    previous_score = score;
    ponggame_render_score(SCORE_X_POS, SCORE_Y_POS, score);
}

/* ----------------------------------------------------------------------------
 * ponggame_render_score()
 *
 *  Helper function to actually render the score on screen
 *
 *  Param:  (x,y) location and Score
 *  return: none
 * 
 */
static void ponggame_render_score(uint32_t x, uint32_t y, int score)
{
    int score_div_ten;
    int score_temp;
    int score_digit;
    int digit_index = 0;

    a_bit_map.col_count = SPRITE_NUMBERS_COLS;
    a_bit_map.row_count = SPRITE_NUMBERS_ROWS;

    score_temp = score;

    do
    {
        score_div_ten = score_temp / 10;
        score_digit = score_temp - (score_div_ten * 10);
        score_temp = score_div_ten;

        a_bit_map.bitmap = &sprite_numbers[(score_digit * SPRITE_NUMBERS_ROWS)];
        video_bit_blit(x - (digit_index * SPRITE_NUMBERS_COLS), y, &a_bit_map);

        digit_index++;
    }
    while ( score_temp != 0 );
}
