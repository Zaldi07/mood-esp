#include "ui/flappy_game.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"

#define W OLED_WIDTH
#define H OLED_HEIGHT

#define BIRD_X 26
#define BIRD_W 8
#define BIRD_H 6
#define BIRD_START_Y 26
#define BIRD_JUMP_VY (-1.9f)
#define BIRD_GRAVITY 0.14f
#define BIRD_MAX_FALL 1.4f

#define PIPE_W 14
#define PIPE_GAP_H 22
#define PIPE_SPEED 1
#define PIPE_SPACING 54
#define PIPE_COUNT 3
#define GROUND_H 6

typedef struct {
    int x;
    int gap_y;
    bool passed;
} pipe_t;

static float s_bird_y;
static float s_bird_vy;
static bool s_game_over;
static bool s_started;
static int s_score;
static pipe_t s_pipes[PIPE_COUNT];
static uint32_t s_rand_state;

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

static uint32_t game_rand(void)
{
    s_rand_state ^= s_rand_state << 13;
    s_rand_state ^= s_rand_state >> 17;
    s_rand_state ^= s_rand_state << 5;
    return s_rand_state;
}

static int next_gap_y(void)
{
    const int min_gap_y = 10;
    const int max_gap_y = H - GROUND_H - PIPE_GAP_H - 10;
    return min_gap_y + (int)(game_rand() % (uint32_t)(max_gap_y - min_gap_y + 1));
}

static void draw_glyph(int x, int y, const uint8_t glyph[5])
{
    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
            if (((glyph[col] >> row) & 1U) == 0U) {
                continue;
            }
            oled_draw_pixel(x + col, y + row);
        }
    }
}

static void draw_number(int x, int y, int value)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", value);
    for (int i = 0; buf[i] != '\0'; ++i) {
        draw_glyph(x + i * 6, y, s_glyph_num[buf[i] - '0']);
    }
}

static void draw_bird(int x, int y)
{
    oled_draw_pixel(x + 1, y + 0);
    oled_draw_pixel(x + 2, y + 0);
    oled_draw_pixel(x + 3, y + 0);
    oled_draw_pixel(x + 4, y + 0);
    oled_draw_pixel(x + 0, y + 1);
    oled_draw_pixel(x + 1, y + 1);
    oled_draw_pixel(x + 2, y + 1);
    oled_draw_pixel(x + 3, y + 1);
    oled_draw_pixel(x + 4, y + 1);
    oled_draw_pixel(x + 5, y + 1);
    oled_draw_pixel(x + 0, y + 2);
    oled_draw_pixel(x + 1, y + 2);
    oled_draw_pixel(x + 2, y + 2);
    oled_draw_pixel(x + 3, y + 2);
    oled_draw_pixel(x + 4, y + 2);
    oled_draw_pixel(x + 5, y + 2);
    oled_draw_pixel(x + 6, y + 2);
    oled_draw_pixel(x + 1, y + 3);
    oled_draw_pixel(x + 2, y + 3);
    oled_draw_pixel(x + 3, y + 3);
    oled_draw_pixel(x + 4, y + 3);
    oled_draw_pixel(x + 5, y + 3);
    oled_draw_pixel(x + 2, y + 4);
    oled_draw_pixel(x + 3, y + 4);
    oled_draw_pixel(x + 4, y + 4);
    oled_draw_pixel(x + 3, y + 5);
    oled_draw_pixel(x + 6, y + 1);
    oled_draw_pixel(x + 7, y + 1);
}

static void draw_pipe(const pipe_t *pipe)
{
    int top_h = pipe->gap_y;
    int bottom_y = pipe->gap_y + PIPE_GAP_H;
    int bottom_h = H - GROUND_H - bottom_y;

    if (top_h > 0) {
        oled_draw_box(pipe->x, 0, PIPE_W, top_h);
        oled_draw_box(pipe->x - 1, top_h - 3, PIPE_W + 2, 3);
    }
    if (bottom_h > 0) {
        oled_draw_box(pipe->x, bottom_y, PIPE_W, bottom_h);
        oled_draw_box(pipe->x - 1, bottom_y, PIPE_W + 2, 3);
    }
}

static bool rect_overlap(int ax, int ay, int aw, int ah,
                         int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static bool bird_hits_pipe(const pipe_t *pipe, int bird_y)
{
    if (!rect_overlap(BIRD_X, bird_y, BIRD_W, BIRD_H, pipe->x, 0, PIPE_W, H - GROUND_H)) {
        return false;
    }

    if (bird_y >= pipe->gap_y && bird_y + BIRD_H <= pipe->gap_y + PIPE_GAP_H) {
        return false;
    }
    return true;
}

void flappy_game_init(void)
{
    s_bird_y = (float)BIRD_START_Y;
    s_bird_vy = 0.0f;
    s_game_over = false;
    s_started = false;
    s_score = 0;
    s_rand_state = (uint32_t)esp_timer_get_time();
    if (s_rand_state == 0U) {
        s_rand_state = 0x12345678U;
    }

    for (int i = 0; i < PIPE_COUNT; ++i) {
        s_pipes[i].x = W + 18 + i * PIPE_SPACING;
        s_pipes[i].gap_y = next_gap_y();
        s_pipes[i].passed = false;
    }
}

void flappy_game_jump(void)
{
    if (s_game_over) {
        flappy_game_init();
        return;
    }
    if (!s_started) {
        s_started = true;
    }
    s_bird_vy = BIRD_JUMP_VY;
}

bool flappy_game_tick(void)
{
    if (!s_game_over && s_started) {
        s_bird_vy += BIRD_GRAVITY;
        if (s_bird_vy > BIRD_MAX_FALL) {
            s_bird_vy = BIRD_MAX_FALL;
        }
        s_bird_y += s_bird_vy;
        if (s_bird_y < 0.0f) {
            s_bird_y = 0.0f;
            s_bird_vy = 0.0f;
        }

        int rightmost_x = 0;
        for (int i = 0; i < PIPE_COUNT; ++i) {
            if (s_pipes[i].x > rightmost_x) {
                rightmost_x = s_pipes[i].x;
            }
            s_pipes[i].x -= PIPE_SPEED;
            if (!s_pipes[i].passed && s_pipes[i].x + PIPE_W < BIRD_X) {
                s_pipes[i].passed = true;
                s_score++;
            }
        }

        for (int i = 0; i < PIPE_COUNT; ++i) {
            if (s_pipes[i].x + PIPE_W >= 0) {
                continue;
            }
            s_pipes[i].x = rightmost_x + PIPE_SPACING;
            s_pipes[i].gap_y = next_gap_y();
            s_pipes[i].passed = false;
            rightmost_x = s_pipes[i].x;
        }

        int bird_y = (int)s_bird_y;
        if (bird_y + BIRD_H >= H - GROUND_H) {
            s_game_over = true;
        }

        for (int i = 0; i < PIPE_COUNT && !s_game_over; ++i) {
            if (bird_hits_pipe(&s_pipes[i], bird_y)) {
                s_game_over = true;
            }
        }
    }

    oled_clear_buffer();

    for (int x = 0; x < W; x += 10) {
        oled_draw_pixel(x, 8 + (x / 10) % 2);
        oled_draw_pixel(x + 4, 16 + ((x / 10) % 3));
    }

    for (int i = 0; i < PIPE_COUNT; ++i) {
        draw_pipe(&s_pipes[i]);
    }

    oled_draw_box(0, H - GROUND_H, W, GROUND_H);
    draw_bird(BIRD_X, (int)s_bird_y);
    draw_number(3, 2, s_score);

    oled_set_font(u8g2_font_my_chinese);
    oled_set_draw_color(1);
    if (s_game_over) {
        oled_draw_UTF8(37, 28, "GAME OVER");
        oled_draw_UTF8(28, 42, "Tap to Retry");
    } else if (!s_started) {
        oled_draw_UTF8(28, 42, "Tap to Start");
    }

    oled_send_buffer();
    return true;
}

bool flappy_game_is_over(void)
{
    return s_game_over;
}

int flappy_game_get_score(void)
{
    return s_score;
}
