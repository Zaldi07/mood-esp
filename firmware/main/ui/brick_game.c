#include "ui/brick_game.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_timer.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"

#define W OLED_WIDTH
#define H OLED_HEIGHT

#define BRICK_ROWS 3
#define BRICK_COLS 7
#define BRICK_W 14
#define BRICK_H 4
#define BRICK_GAP_X 3
#define BRICK_GAP_Y 3
#define BRICK_OFFSET_X 6
#define BRICK_OFFSET_Y 10

#define PADDLE_W 22
#define PADDLE_H 3
#define PADDLE_Y (H - 6)
#define PADDLE_SPEED 3
#define PADDLE_DEADZONE 220

#define BALL_R 2
#define BALL_SPEED_X 1
#define BALL_SPEED_Y -1

#define MAX_LIVES 3

static bool s_bricks[BRICK_ROWS][BRICK_COLS];
static int s_paddle_x;
static int s_ball_x;
static int s_ball_y;
static int s_ball_vx;
static int s_ball_vy;
static int32_t s_tilt_lr;
static int s_score;
static int s_lives;
static bool s_finished;
static bool s_win;

static const uint8_t s_glyph_num[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static const uint8_t s_glyph_s[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
static const uint8_t s_glyph_l[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};

static void reset_ball(void)
{
    s_ball_x = s_paddle_x + PADDLE_W / 2;
    s_ball_y = PADDLE_Y - BALL_R - 1;
    s_ball_vx = BALL_SPEED_X;
    s_ball_vy = BALL_SPEED_Y;
}

static void reset_level(void)
{
    memset(s_bricks, 1, sizeof(s_bricks));
    s_paddle_x = (W - PADDLE_W) / 2;
    s_score = 0;
    s_lives = MAX_LIVES;
    s_finished = false;
    s_win = false;
    s_tilt_lr = 0;
    reset_ball();
}

static int clamp_paddle_step(int32_t tilt)
{
    if (tilt > -PADDLE_DEADZONE && tilt < PADDLE_DEADZONE) {
        return 0;
    }

    int step = PADDLE_SPEED;
    if (tilt >= PADDLE_DEADZONE * 3 || tilt <= -PADDLE_DEADZONE * 3) {
        step = PADDLE_SPEED + 1;
    }
    return tilt > 0 ? step : -step;
}

static void draw_glyph(int x, int y, const uint8_t glyph[5])
{
    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
            if (!((glyph[col] >> row) & 1U)) {
                continue;
            }
            oled_draw_pixel(x + col, y + row);
        }
    }
}

static void draw_number(int x, int y, int num)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", num);
    for (int i = 0; buf[i] != '\0'; ++i) {
        draw_glyph(x + i * 6, y, s_glyph_num[buf[i] - '0']);
    }
}

static void draw_hud(void)
{
    draw_glyph(1, 0, s_glyph_s);
    draw_number(8, 0, s_score);

    draw_glyph(W - 22, 0, s_glyph_l);
    draw_number(W - 14, 0, s_lives);
}

static void draw_bricks(void)
{
    for (int row = 0; row < BRICK_ROWS; ++row) {
        for (int col = 0; col < BRICK_COLS; ++col) {
            if (!s_bricks[row][col]) {
                continue;
            }
            int x = BRICK_OFFSET_X + col * (BRICK_W + BRICK_GAP_X);
            int y = BRICK_OFFSET_Y + row * (BRICK_H + BRICK_GAP_Y);
            oled_draw_frame(x, y, BRICK_W, BRICK_H);
            if (BRICK_W > 4 && BRICK_H > 2) {
                oled_draw_H_line(x + 2, y + 1, BRICK_W - 4);
            }
        }
    }
}

static void draw_paddle(void)
{
    oled_draw_box(s_paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H);
}

static void draw_ball(void)
{
    for (int dy = -BALL_R; dy <= BALL_R; ++dy) {
        for (int dx = -BALL_R; dx <= BALL_R; ++dx) {
            if (dx * dx + dy * dy > BALL_R * BALL_R) {
                continue;
            }
            oled_draw_pixel(s_ball_x + dx, s_ball_y + dy);
        }
    }
}

static bool bricks_remaining(void)
{
    for (int row = 0; row < BRICK_ROWS; ++row) {
        for (int col = 0; col < BRICK_COLS; ++col) {
            if (s_bricks[row][col]) {
                return true;
            }
        }
    }
    return false;
}

static void update_paddle(void)
{
    s_paddle_x += clamp_paddle_step(s_tilt_lr);
    if (s_paddle_x < 0) {
        s_paddle_x = 0;
    }
    if (s_paddle_x > W - PADDLE_W) {
        s_paddle_x = W - PADDLE_W;
    }
}

void brick_game_step_left(void)
{
    s_paddle_x -= PADDLE_SPEED + 1;
    if (s_paddle_x < 0) {
        s_paddle_x = 0;
    }
}

void brick_game_step_right(void)
{
    s_paddle_x += PADDLE_SPEED + 1;
    if (s_paddle_x > W - PADDLE_W) {
        s_paddle_x = W - PADDLE_W;
    }
}

static void handle_brick_collision(int next_x, int next_y)
{
    for (int row = 0; row < BRICK_ROWS; ++row) {
        for (int col = 0; col < BRICK_COLS; ++col) {
            if (!s_bricks[row][col]) {
                continue;
            }

            int bx = BRICK_OFFSET_X + col * (BRICK_W + BRICK_GAP_X);
            int by = BRICK_OFFSET_Y + row * (BRICK_H + BRICK_GAP_Y);
            bool overlap_x = next_x + BALL_R >= bx && next_x - BALL_R < bx + BRICK_W;
            bool overlap_y = next_y + BALL_R >= by && next_y - BALL_R < by + BRICK_H;
            if (!overlap_x || !overlap_y) {
                continue;
            }

            s_bricks[row][col] = false;
            s_score += 10;
            s_ball_vy = -s_ball_vy;
            return;
        }
    }
}

static void update_ball(void)
{
    int next_x = s_ball_x + s_ball_vx;
    int next_y = s_ball_y + s_ball_vy;

    if (next_x - BALL_R <= 0 || next_x + BALL_R >= W - 1) {
        s_ball_vx = -s_ball_vx;
        next_x = s_ball_x + s_ball_vx;
    }

    if (next_y - BALL_R <= 0) {
        s_ball_vy = -s_ball_vy;
        next_y = s_ball_y + s_ball_vy;
    }

    bool hit_paddle_y = next_y + BALL_R >= PADDLE_Y &&
                        s_ball_y <= PADDLE_Y;
    bool hit_paddle_x = next_x + BALL_R >= s_paddle_x &&
                        next_x - BALL_R <= s_paddle_x + PADDLE_W;
    if (hit_paddle_y && hit_paddle_x && s_ball_vy > 0) {
        int relative = next_x - (s_paddle_x + PADDLE_W / 2);
        s_ball_vx = relative / 4;
        if (s_ball_vx == 0) {
            s_ball_vx = (relative >= 0) ? 1 : -1;
        }
        if (s_ball_vx > 2) s_ball_vx = 2;
        if (s_ball_vx < -2) s_ball_vx = -2;
        s_ball_vy = -1;
        next_y = PADDLE_Y - BALL_R - 1;
    }

    handle_brick_collision(next_x, next_y);

    s_ball_x += s_ball_vx;
    s_ball_y += s_ball_vy;

    if (s_ball_y - BALL_R > H) {
        s_lives--;
        if (s_lives <= 0) {
            s_finished = true;
            s_win = false;
            return;
        }
        reset_ball();
    }

    if (!bricks_remaining()) {
        s_finished = true;
        s_win = true;
    }
}

void brick_game_init(void)
{
    reset_level();
}

void brick_game_set_tilt(int32_t lr_val)
{
    s_tilt_lr = lr_val;
}

bool brick_game_tick(void)
{
    if (!s_finished) {
        update_paddle();
        update_ball();
    }

    oled_clear_buffer();
    draw_bricks();
    draw_paddle();
    draw_ball();
    draw_hud();

    oled_set_font(u8g2_font_my_chinese);
    oled_set_draw_color(1);
    if (s_finished) {
        if (s_win) {
            oled_draw_UTF8(40, 30, "YOU WIN!");
        } else {
            oled_draw_UTF8(37, 30, "GAME OVER");
        }
    }
    oled_send_buffer();
    return true;
}

bool brick_game_is_finished(void)
{
    return s_finished;
}
