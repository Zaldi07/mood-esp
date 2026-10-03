#include "ui/pomodoro.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_log.h"
#include "drivers/buzzer.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"

static const char *TAG = "pomodoro";

#define FOCUS_DURATION_SEC     (25 * 60)
#define BREAK_DURATION_SEC     (5 * 60)
#define LONG_BREAK_DURATION_SEC (15 * 60)

static pomodoro_phase_t s_phase = POMODORO_PHASE_FOCUS;
static pomodoro_state_t s_state = POMODORO_STATE_PAUSED;
static uint32_t s_remaining_seconds = FOCUS_DURATION_SEC;
static uint32_t s_total_seconds     = FOCUS_DURATION_SEC;
static uint16_t s_completed_cycles  = 0;
static int64_t  s_last_tick_us      = 0;
static bool     s_blink             = false;
static int64_t  s_last_blink_us     = 0;

/* 5x7 font bitmaps for digits '0'-'9' and ':' */
static const uint8_t s_glyph_digits[11][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* : (colon) */
};

static void draw_scaled_char(int x, int y, char c, int scale)
{
    const uint8_t *glyph = NULL;
    if (c >= '0' && c <= '9') {
        glyph = s_glyph_digits[c - '0'];
    } else if (c == ':') {
        glyph = s_glyph_digits[10];
    } else {
        return;
    }

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if ((glyph[col] >> row) & 1U) {
                oled_draw_box(x + col * scale, y + row * scale, scale, scale);
            }
        }
    }
}

static void draw_scaled_string(int x, int y, const char *str, int scale)
{
    int spacing = (scale >= 3) ? 3 : 2;
    int char_w = 5 * scale + spacing;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_scaled_char(x + i * char_w, y, str[i], scale);
    }
}

static void draw_mini_tomato(int x, int y)
{
    /* Cute 7x7 tomato icon */
    oled_draw_pixel(x + 1, y);
    oled_draw_pixel(x + 5, y);
    oled_draw_pixel(x + 3, y + 1);
    oled_draw_box(x + 1, y + 2, 5, 4);
    oled_draw_box(x + 2, y + 6, 3, 1);
    oled_draw_pixel(x + 0, y + 3);
    oled_draw_pixel(x + 0, y + 4);
    oled_draw_pixel(x + 6, y + 3);
    oled_draw_pixel(x + 6, y + 4);
}

void pomodoro_init(void)
{
    s_phase = POMODORO_PHASE_FOCUS;
    s_state = POMODORO_STATE_PAUSED;
    s_total_seconds = FOCUS_DURATION_SEC;
    s_remaining_seconds = FOCUS_DURATION_SEC;
    s_last_tick_us = esp_timer_get_time();
    s_last_blink_us = s_last_tick_us;
    s_blink = true;
    buzzer_stop();
    ESP_LOGI(TAG, "Pomodoro timer initialized (25m Focus / 5m Break)");
}

void pomodoro_tap(void)
{
    int64_t now_us = esp_timer_get_time();

    /* Stop alarm immediately if user taps */
    buzzer_stop();

    if (s_state == POMODORO_STATE_DONE) {
        /* Session completed: advance to next phase */
        if (s_phase == POMODORO_PHASE_FOCUS) {
            s_phase = POMODORO_PHASE_BREAK;
            if (s_completed_cycles > 0 && (s_completed_cycles % 4 == 0)) {
                s_total_seconds = LONG_BREAK_DURATION_SEC;
            } else {
                s_total_seconds = BREAK_DURATION_SEC;
            }
        } else {
            s_phase = POMODORO_PHASE_FOCUS;
            s_total_seconds = FOCUS_DURATION_SEC;
        }
        s_remaining_seconds = s_total_seconds;
        s_state = POMODORO_STATE_RUNNING;
        s_last_tick_us = now_us;
        ESP_LOGI(TAG, "Pomodoro: started next phase %s (%lu sec)",
                 s_phase == POMODORO_PHASE_FOCUS ? "Focus" : "Break",
                 (unsigned long)s_total_seconds);
        return;
    }

    if (s_state == POMODORO_STATE_PAUSED) {
        s_state = POMODORO_STATE_RUNNING;
        s_last_tick_us = now_us;
        ESP_LOGI(TAG, "Pomodoro: timer resumed (rem=%lu sec)",
                 (unsigned long)s_remaining_seconds);
    } else {
        s_state = POMODORO_STATE_PAUSED;
        ESP_LOGI(TAG, "Pomodoro: timer paused (rem=%lu sec)",
                 (unsigned long)s_remaining_seconds);
    }
}

void pomodoro_double_tap(void)
{
    buzzer_stop();

    /* Double tap: Reset session time, or toggle phase if already at start */
    if (s_state == POMODORO_STATE_PAUSED) {
        if (s_remaining_seconds < s_total_seconds) {
            s_remaining_seconds = s_total_seconds;
            ESP_LOGI(TAG, "Pomodoro: reset to %lu sec", (unsigned long)s_total_seconds);
        } else {
            /* Already full -> toggle phase */
            if (s_phase == POMODORO_PHASE_FOCUS) {
                s_phase = POMODORO_PHASE_BREAK;
                s_total_seconds = BREAK_DURATION_SEC;
            } else {
                s_phase = POMODORO_PHASE_FOCUS;
                s_total_seconds = FOCUS_DURATION_SEC;
            }
            s_remaining_seconds = s_total_seconds;
            ESP_LOGI(TAG, "Pomodoro: toggled phase to %s",
                     s_phase == POMODORO_PHASE_FOCUS ? "Focus" : "Break");
        }
    } else if (s_state == POMODORO_STATE_RUNNING) {
        /* Pause and reset */
        s_state = POMODORO_STATE_PAUSED;
        s_remaining_seconds = s_total_seconds;
        ESP_LOGI(TAG, "Pomodoro: paused and reset to %lu sec", (unsigned long)s_total_seconds);
    }
}

uint32_t pomodoro_get_remaining_sec(void)
{
    return s_remaining_seconds;
}

bool pomodoro_is_running(void)
{
    return (s_state == POMODORO_STATE_RUNNING);
}

uint16_t pomodoro_get_completed_cycles(void)
{
    return s_completed_cycles;
}

void pomodoro_tick(void)
{
    int64_t now_us = esp_timer_get_time();

    /* Blink update (every 500ms) */
    if (now_us - s_last_blink_us >= 500000LL) {
        s_blink = !s_blink;
        s_last_blink_us = now_us;
    }

    /* Update active buzzer alarm sound pattern */
    buzzer_tick();

    /* Countdown logic */
    if (s_state == POMODORO_STATE_RUNNING) {
        if (now_us - s_last_tick_us >= 1000000LL) {
            uint32_t elapsed_sec = (uint32_t)((now_us - s_last_tick_us) / 1000000LL);
            s_last_tick_us += (int64_t)elapsed_sec * 1000000LL;
            if (s_remaining_seconds > elapsed_sec) {
                s_remaining_seconds -= elapsed_sec;
            } else {
                s_remaining_seconds = 0;
                s_state = POMODORO_STATE_DONE;
                if (s_phase == POMODORO_PHASE_FOCUS) {
                    s_completed_cycles++;
                }
                buzzer_start_alarm();
                ESP_LOGI(TAG, "Pomodoro: 25 minutes session finished! Alarm ringing on pin 5. Cycle count: %u",
                         s_completed_cycles);
            }
        }
    } else {
        s_last_tick_us = now_us;
    }

    /* Rendering */
    oled_clear_buffer();
    oled_set_draw_color(1);
    oled_set_font(u8g2_font_my_chinese);

    /* 1. Header Bar */
    if (s_phase == POMODORO_PHASE_FOCUS) {
        oled_draw_R_box(1, 1, 56, 11, 2);
        oled_set_draw_color(0);
        oled_draw_UTF8(4, 10, "FOCUS 25M");
        oled_set_draw_color(1);
    } else {
        oled_draw_R_box(1, 1, 56, 11, 2);
        oled_set_draw_color(0);
        if (s_total_seconds >= LONG_BREAK_DURATION_SEC) {
            oled_draw_UTF8(4, 10, "BREAK 15M");
        } else {
            oled_draw_UTF8(4, 10, "BREAK 5M");
        }
        oled_set_draw_color(1);
    }

    /* Tomato icon and cycle counter */
    draw_mini_tomato(OLED_WIDTH - 36, 3);
    char cycle_buf[16];
    snprintf(cycle_buf, sizeof(cycle_buf), "x%u", (unsigned int)s_completed_cycles);
    oled_draw_UTF8(OLED_WIDTH - 26, 10, cycle_buf);

    /* Header separator dotted line */
    oled_draw_H_dotted_line(0, 13, OLED_WIDTH);

    /* 2. Large Digital Countdown Display */
    uint32_t min = s_remaining_seconds / 60;
    uint32_t sec = s_remaining_seconds % 60;
    char time_str[32];

    if (s_state == POMODORO_STATE_PAUSED && !s_blink) {
        /* Blink colon when paused to indicate standby */
        snprintf(time_str, sizeof(time_str), "%02u %02u",
                 (unsigned int)min, (unsigned int)sec);
    } else {
        snprintf(time_str, sizeof(time_str), "%02u:%02u",
                 (unsigned int)min, (unsigned int)sec);
    }

    /* 5 characters * 18px = 87px width. Center at (128 - 87) / 2 = 20 */
    draw_scaled_string(21, 16, time_str, 3);

    /* 3. Status Subtitle Banner */
    if (s_state == POMODORO_STATE_DONE) {
        if (buzzer_is_alarm_active()) {
            if (s_blink) {
                oled_draw_UTF8(10, 46, "ALARM! TIME'S UP!");
            } else {
                oled_draw_UTF8(14, 46, "Tap to Stop Alarm");
            }
        } else {
            if (s_blink) {
                oled_draw_UTF8(12, 46, "** SESSION FINISHED! **");
            } else {
                oled_draw_UTF8(16, 46, ">> Tap to Continue <<");
            }
        }
    } else if (s_state == POMODORO_STATE_PAUSED) {
        oled_draw_UTF8(16, 46, "PAUSED (Tap: Start)");
    } else {
        if (s_phase == POMODORO_PHASE_FOCUS) {
            oled_draw_UTF8(24, 46, "Focusing Time...");
        } else {
            oled_draw_UTF8(24, 46, "Rest & Relax...");
        }
    }

    /* 4. Progress Bar */
    int bar_x = 10;
    int bar_y = 48;
    int bar_w = OLED_WIDTH - 20;
    int bar_h = 4;
    oled_draw_frame(bar_x, bar_y, bar_w, bar_h);

    if (s_total_seconds > 0) {
        uint32_t elapsed = s_total_seconds - s_remaining_seconds;
        int fill_w = (int)((elapsed * (bar_w - 2)) / s_total_seconds);
        if (fill_w > bar_w - 2) fill_w = bar_w - 2;
        if (fill_w > 0) {
            oled_draw_box(bar_x + 1, bar_y + 1, fill_w, bar_h - 2);
        }
    }

    /* 5. Footer Instructions */
    if (s_state == POMODORO_STATE_DONE) {
        oled_draw_UTF8(18, 62, "Tap: Next  Hold: Exit");
    } else if (s_state == POMODORO_STATE_PAUSED) {
        oled_draw_UTF8(14, 62, "Tap: Run  2xTap: Reset");
    } else {
        oled_draw_UTF8(16, 62, "Tap: Pause  Hold: Exit");
    }

    oled_send_buffer();
}
