#include "ui/carmood_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "app/time_sync.h"
#include "ssd1306_oled.h"
#include "ui/face_renderer.h"
#include "ui/muyu_renderer.h"
#include "ui/brick_game.h"
#include "ui/flappy_game.h"
#include "ui/shooter_game.h"

static const char *TAG = "carmood_ui";

/* 目标 30fps = 33ms/帧，OLED 上已足够丝滑 */
#define ANIM_FRAME_MS 33
/* 表情状态切换时保留一个很短的过渡窗，弱化硬切感。 */
#define FACE_TRANSITION_MS 180
/* 共享状态（由 mutex 保护） */
static SemaphoreHandle_t s_mutex;
static volatile carmood_expr_t s_cur_expr     = EXPR_IDLE;
static volatile carmood_expr_t s_pending_expr = EXPR_IDLE;
static volatile bool           s_expr_changed = false;
static volatile bool           s_anim_paused  = false;
static volatile carmood_display_mode_t s_display_mode = CARMOOD_DISPLAY_MODE_FACE;
static volatile bool           s_debug_mode   = false;
static char                    s_turn_char    = 'C';
static char                    s_pitch_char   = 'N';
static int32_t                 s_motion_lr_val = 0;
static int32_t                 s_motion_fb_val = 0;
static int32_t                 s_motion_shake_val = 0;

/* 木鱼模式状态 */
static volatile bool    s_muyu_mode      = false;
static volatile bool    s_muyu_tapped    = false;
static volatile bool    s_muyu_animating = false;
static volatile int     s_muyu_count     = 0;
static int64_t          s_muyu_tap_start = 0;

/* 打飞机游戏模式状态 */
static volatile bool    s_shooter_mode   = false;
static volatile bool    s_brick_mode     = false;
static volatile bool    s_flappy_mode    = false;

/* ------------------------------------------------------------------ */
/* 5x7 字体（校准流程用）                                               */
/* ------------------------------------------------------------------ */

static bool get_glyph_5x7(char c, uint8_t glyph[5])
{
    switch (c) {
        case 'A': { uint8_t g[5]={0x7E,0x09,0x09,0x09,0x7E}; memcpy(glyph,g,5); return true; }
        case 'B': { uint8_t g[5]={0x7F,0x49,0x49,0x49,0x36}; memcpy(glyph,g,5); return true; }
        case 'C': { uint8_t g[5]={0x3E,0x41,0x41,0x41,0x22}; memcpy(glyph,g,5); return true; }
        case 'D': { uint8_t g[5]={0x7F,0x41,0x41,0x22,0x1C}; memcpy(glyph,g,5); return true; }
        case 'E': { uint8_t g[5]={0x7F,0x49,0x49,0x49,0x41}; memcpy(glyph,g,5); return true; }
        case 'F': { uint8_t g[5]={0x7F,0x09,0x09,0x09,0x01}; memcpy(glyph,g,5); return true; }
        case 'G': { uint8_t g[5]={0x3E,0x41,0x49,0x49,0x7A}; memcpy(glyph,g,5); return true; }
        case 'H': { uint8_t g[5]={0x7F,0x08,0x08,0x08,0x7F}; memcpy(glyph,g,5); return true; }
        case 'I': { uint8_t g[5]={0x00,0x41,0x7F,0x41,0x00}; memcpy(glyph,g,5); return true; }
        case 'K': { uint8_t g[5]={0x7F,0x08,0x14,0x22,0x41}; memcpy(glyph,g,5); return true; }
        case 'L': { uint8_t g[5]={0x7F,0x40,0x40,0x40,0x40}; memcpy(glyph,g,5); return true; }
        case 'M': { uint8_t g[5]={0x7F,0x02,0x0C,0x02,0x7F}; memcpy(glyph,g,5); return true; }
        case 'N': { uint8_t g[5]={0x7F,0x02,0x0C,0x10,0x7F}; memcpy(glyph,g,5); return true; }
        case 'O': { uint8_t g[5]={0x3E,0x41,0x41,0x41,0x3E}; memcpy(glyph,g,5); return true; }
        case 'P': { uint8_t g[5]={0x7F,0x09,0x09,0x09,0x06}; memcpy(glyph,g,5); return true; }
        case 'R': { uint8_t g[5]={0x7F,0x09,0x19,0x29,0x46}; memcpy(glyph,g,5); return true; }
        case 'S': { uint8_t g[5]={0x46,0x49,0x49,0x49,0x31}; memcpy(glyph,g,5); return true; }
        case 'T': { uint8_t g[5]={0x01,0x01,0x7F,0x01,0x01}; memcpy(glyph,g,5); return true; }
        case 'U': { uint8_t g[5]={0x3F,0x40,0x40,0x40,0x3F}; memcpy(glyph,g,5); return true; }
        case 'W': { uint8_t g[5]={0x7F,0x20,0x18,0x20,0x7F}; memcpy(glyph,g,5); return true; }
        case ' ': { uint8_t g[5]={0x00,0x00,0x00,0x00,0x00}; memcpy(glyph,g,5); return true; }
        case '0': { uint8_t g[5]={0x3E,0x51,0x49,0x45,0x3E}; memcpy(glyph,g,5); return true; }
        case '1': { uint8_t g[5]={0x00,0x42,0x7F,0x40,0x00}; memcpy(glyph,g,5); return true; }
        case '2': { uint8_t g[5]={0x42,0x61,0x51,0x49,0x46}; memcpy(glyph,g,5); return true; }
        case '3': { uint8_t g[5]={0x21,0x41,0x45,0x4B,0x31}; memcpy(glyph,g,5); return true; }
        case '4': { uint8_t g[5]={0x18,0x14,0x12,0x7F,0x10}; memcpy(glyph,g,5); return true; }
        case '5': { uint8_t g[5]={0x27,0x45,0x45,0x45,0x39}; memcpy(glyph,g,5); return true; }
        case '6': { uint8_t g[5]={0x3C,0x4A,0x49,0x49,0x30}; memcpy(glyph,g,5); return true; }
        case '7': { uint8_t g[5]={0x01,0x71,0x09,0x05,0x03}; memcpy(glyph,g,5); return true; }
        case '8': { uint8_t g[5]={0x36,0x49,0x49,0x49,0x36}; memcpy(glyph,g,5); return true; }
        case '9': { uint8_t g[5]={0x06,0x49,0x49,0x29,0x1E}; memcpy(glyph,g,5); return true; }
        case ':': { uint8_t g[5]={0x00,0x36,0x36,0x00,0x00}; memcpy(glyph,g,5); return true; }
        default:    return false;
    }
}

static void draw_char(int x, int y, char c, int scale)
{
    uint8_t glyph[5] = {0};
    if (!get_glyph_5x7(c, glyph)) return;
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++) {
            if (!((glyph[col] >> row) & 0x01U)) continue;
            for (int sx = 0; sx < scale; sx++)
                for (int sy = 0; sy < scale; sy++)
                    oled_set_pixel(x + col * scale + sx, y + row * scale + sy, true);
        }
}

static void draw_text(int x, int y, const char *text, int scale)
{
    int cw = 6 * scale;
    for (int i = 0; text[i]; i++)
        draw_char(x + i * cw, y, text[i], scale);
}

static void draw_text_center(int y, const char *text, int scale)
{
    int len = (int)strlen(text);
    int x   = (OLED_WIDTH - len * 6 * scale) / 2;
    if (x < 0) x = 0;
    draw_text(x, y, text, scale);
}

/* ------------------------------------------------------------------ */
/* 方向叠加层                                                          */
/* ------------------------------------------------------------------ */

static void draw_overlay(char tc, char pc)
{
    for (int py = 0; py < 8; py++)
        for (int px = 0; px < 28; px++)
            oled_set_pixel(px, py, false);
    char text[4] = {tc, ' ', pc, '\0'};
    draw_text(0, 0, text, 1);
}

/* ------------------------------------------------------------------ */
/* 动画后台任务（Core 1，30fps 实时绘制）                                */
/* ------------------------------------------------------------------ */

static void draw_clock(int64_t now_us)
{
    char buf[6] = "00:00";
    time_t now_s = time(NULL);
    bool is_time_valid = (now_s >= 1704067200); /* >= 2024-01-01，认为已完成校时 */
    if (is_time_valid) {
        struct tm tm_now;
        localtime_r(&now_s, &tm_now);
        buf[0] = '0' + (tm_now.tm_hour / 10);
        buf[1] = '0' + (tm_now.tm_hour % 10);
        buf[3] = '0' + (tm_now.tm_min / 10);
        buf[4] = '0' + (tm_now.tm_min % 10);
    } else {
        /* 未校时时，显示运行时长 MM:SS */
        uint32_t sec = (uint32_t)(now_us / 1000000ULL);
        uint32_t mm = (sec / 60U) % 100U;
        uint32_t ss = sec % 60U;
        buf[0] = '0' + (mm / 10U);
        buf[1] = '0' + (mm % 10U);
        buf[3] = '0' + (ss / 10U);
        buf[4] = '0' + (ss % 10U);
    }

    int x = OLED_WIDTH - 5 * 6;
    bool wifi_ready = carmood_time_sync_is_synced() || is_time_valid;
    int wifi_x = x - 10;
    for (int py = 0; py < 8; py++)
        for (int px = wifi_x - 1; px < OLED_WIDTH; px++)
            oled_set_pixel(px, py, false);

    if (wifi_ready) {
        oled_set_pixel(wifi_x + 4, 7, true);
        oled_set_pixel(wifi_x + 2, 5, true);
        oled_set_pixel(wifi_x + 3, 4, true);
        oled_set_pixel(wifi_x + 4, 3, true);
        oled_set_pixel(wifi_x + 5, 4, true);
        oled_set_pixel(wifi_x + 6, 5, true);
        oled_set_pixel(wifi_x + 1, 3, true);
        oled_set_pixel(wifi_x + 2, 2, true);
        oled_set_pixel(wifi_x + 3, 1, true);
        oled_set_pixel(wifi_x + 4, 1, true);
        oled_set_pixel(wifi_x + 5, 1, true);
        oled_set_pixel(wifi_x + 6, 2, true);
        oled_set_pixel(wifi_x + 7, 3, true);
    }

    draw_text(x, 0, buf, 1);
}

static void draw_clock_fullscreen(int64_t now_us, bool force)
{
    static char s_last_buf[6] = "";
    static bool s_last_wifi_ready = false;
    char buf[6] = "00:00";
    time_t now_s = time(NULL);
    bool is_time_valid = (now_s >= 1704067200);

    if (is_time_valid) {
        struct tm tm_now;
        localtime_r(&now_s, &tm_now);
        buf[0] = '0' + (tm_now.tm_hour / 10);
        buf[1] = '0' + (tm_now.tm_hour % 10);
        buf[3] = '0' + (tm_now.tm_min / 10);
        buf[4] = '0' + (tm_now.tm_min % 10);
    } else {
        uint32_t sec = (uint32_t)(now_us / 1000000ULL);
        uint32_t mm = (sec / 60U) % 100U;
        uint32_t ss = sec % 60U;
        buf[0] = '0' + (mm / 10U);
        buf[1] = '0' + (mm % 10U);
        buf[3] = '0' + (ss / 10U);
        buf[4] = '0' + (ss % 10U);
    }

    bool wifi_ready = carmood_time_sync_is_synced() || is_time_valid;
    if (!force &&
        strcmp(buf, s_last_buf) == 0 &&
        wifi_ready == s_last_wifi_ready) {
        return;
    }

    strcpy(s_last_buf, buf);
    s_last_wifi_ready = wifi_ready;

    oled_clear_buf();
    draw_text_center(18, buf, 3);

    if (wifi_ready) {
        draw_text_center(46, "TIME", 1);
    } else {
        draw_text_center(46, "UP", 1);
    }

    oled_flush();
}

static void anim_task(void *arg)
{
    (void)arg;
    carmood_expr_t cur_expr = EXPR_IDLE;
    carmood_expr_t prev_expr = EXPR_IDLE;
    int64_t anim_start_us = esp_timer_get_time();
    int64_t prev_anim_start_us = anim_start_us;
    int64_t transition_start_us = 0;
    bool face_transition_active = false;
    carmood_display_mode_t last_display_mode = CARMOOD_DISPLAY_MODE_FACE;

    ESP_LOGI(TAG, "动画任务启动 (实时绘制)");

    while (1) {
        int64_t frame_start_us = esp_timer_get_time();

        /* 校准期间暂停 */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool paused = s_anim_paused;
        xSemaphoreGive(s_mutex);
        if (paused) {
            vTaskDelay(pdMS_TO_TICKS(ANIM_FRAME_MS));
            continue;
        }

        /* 木鱼模式 */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool in_muyu = s_muyu_mode;
        bool muyu_new_tap = s_muyu_tapped;
        bool muyu_anim = s_muyu_animating;
        int  muyu_count = s_muyu_count;
        if (muyu_new_tap) {
            s_muyu_tapped = false;
            s_muyu_animating = true;
            s_muyu_count++;
            muyu_count = s_muyu_count;
            s_muyu_tap_start = esp_timer_get_time();
            muyu_anim = true;
        }
        int64_t tap_start = s_muyu_tap_start;
        xSemaphoreGive(s_mutex);

        if (in_muyu) {
            if (muyu_anim) {
                uint32_t t_ms = (uint32_t)((esp_timer_get_time() - tap_start) / 1000);
                bool still_playing = muyu_render_tap_frame(t_ms, muyu_count);
                if (!still_playing) {
                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                    s_muyu_animating = false;
                    xSemaphoreGive(s_mutex);
                }
                vTaskDelay(pdMS_TO_TICKS(ANIM_FRAME_MS));
            } else {
                muyu_render_idle(muyu_count);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        /* 打飞机游戏模式 */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool in_shooter = s_shooter_mode;
        xSemaphoreGive(s_mutex);

        if (in_shooter) {
            shooter_game_tick();
            vTaskDelay(pdMS_TO_TICKS(ANIM_FRAME_MS));
            continue;
        }

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool in_brick = s_brick_mode;
        xSemaphoreGive(s_mutex);

        if (in_brick) {
            brick_game_tick();
            vTaskDelay(pdMS_TO_TICKS(ANIM_FRAME_MS));
            continue;
        }

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        bool in_flappy = s_flappy_mode;
        xSemaphoreGive(s_mutex);

        if (in_flappy) {
            flappy_game_tick();
            vTaskDelay(pdMS_TO_TICKS(ANIM_FRAME_MS));
            continue;
        }

        /* 检查表情切换 */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_expr_changed) {
            int64_t now_us = esp_timer_get_time();
            prev_expr = cur_expr;
            prev_anim_start_us = anim_start_us;
            cur_expr       = s_pending_expr;
            s_cur_expr     = cur_expr;
            s_expr_changed = false;
            anim_start_us  = now_us;
            transition_start_us = now_us;
            face_transition_active = (prev_expr != cur_expr);
            ESP_LOGI(TAG, "切换表情: %d", cur_expr);
        }
        carmood_display_mode_t display_mode = s_display_mode;
        bool debug_mode = s_debug_mode;
        char tc = s_turn_char;
        char pc = s_pitch_char;
        xSemaphoreGive(s_mutex);

        if (display_mode == CARMOOD_DISPLAY_MODE_CLOCK) {
            draw_clock_fullscreen(frame_start_us, last_display_mode != display_mode);
            last_display_mode = display_mode;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        last_display_mode = display_mode;

        /* 计算动画时间并实时绘制 */
        int64_t now_us = esp_timer_get_time();
        uint32_t t_ms = (uint32_t)((now_us - anim_start_us) / 1000);
        if (face_transition_active) {
            uint32_t prev_t_ms = (uint32_t)((now_us - prev_anim_start_us) / 1000);
            uint32_t transition_ms = (uint32_t)((now_us - transition_start_us) / 1000);
            if (transition_ms >= FACE_TRANSITION_MS) {
                face_transition_active = false;
                face_render_frame(cur_expr, t_ms);
            } else {
                uint8_t progress = (uint8_t)((transition_ms * 255U) / FACE_TRANSITION_MS);
                face_render_transition(prev_expr, prev_t_ms, cur_expr, t_ms, progress);
            }
        } else {
            face_render_frame(cur_expr, t_ms);
        }
        if (debug_mode) {
            draw_overlay(tc, pc);
        }

        draw_clock(frame_start_us);

        oled_flush();

        /* 精确帧间隔：扣除本帧已耗时间，+1 tick 补偿 pdMS_TO_TICKS 截断误差 */
        int64_t elapsed_ms = (esp_timer_get_time() - frame_start_us) / 1000;
        int delay_ms = ANIM_FRAME_MS - (int)elapsed_ms;
        if (delay_ms > 0) {
            TickType_t ticks = pdMS_TO_TICKS(delay_ms);
            if (ticks == 0) ticks = 1;
            vTaskDelay(ticks);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 公开接口                                                             */
/* ------------------------------------------------------------------ */

void carmood_ui_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(anim_task, "anim", 6144, NULL, 5, NULL, 1);
}

void carmood_ui_set_expression(carmood_expr_t expr)
{
    if (expr >= EXPR_COUNT) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_pending_expr = expr;
    s_expr_changed = true;
    xSemaphoreGive(s_mutex);
}

void carmood_ui_set_direction_overlay(char turn_char, char pitch_char)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_turn_char  = turn_char;
    s_pitch_char = pitch_char;
    xSemaphoreGive(s_mutex);
}

void carmood_ui_set_motion_input(int32_t lr_val, int32_t fb_val, int32_t shake_val)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_motion_lr_val = lr_val;
    s_motion_fb_val = fb_val;
    s_motion_shake_val = shake_val;
    xSemaphoreGive(s_mutex);
}

void carmood_ui_show_calibration(const char *line1, const char *line2)
{
    carmood_ui_pause_animation();

    oled_clear();
    draw_text_center(10, line1, 2);
    draw_text_center(34, line2, 2);
    oled_flush();
}

void carmood_ui_pause_animation(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_anim_paused = true;
    xSemaphoreGive(s_mutex);

    vTaskDelay(pdMS_TO_TICKS(ANIM_FRAME_MS + 10));
}

void carmood_ui_resume_animation(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_anim_paused  = false;
    s_expr_changed = true;
    xSemaphoreGive(s_mutex);
}

void carmood_ui_enter_muyu(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_muyu_mode      = true;
    s_muyu_animating = false;
    s_muyu_tapped    = false;
    /* 特殊模式必须互斥，避免木鱼和打飞机同时生效。 */
    s_shooter_mode   = false;
    s_brick_mode     = false;
    s_flappy_mode    = false;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "进入木鱼模式");
}

void carmood_ui_exit_muyu(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_muyu_mode      = false;
    s_muyu_animating = false;
    s_expr_changed   = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "退出木鱼模式");
}

void carmood_ui_tap_muyu(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_muyu_mode) {
        s_muyu_tapped = true;
    }
    xSemaphoreGive(s_mutex);
}

bool carmood_ui_is_muyu_mode(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ret = s_muyu_mode;
    xSemaphoreGive(s_mutex);
    return ret;
}

void carmood_ui_set_display_mode(carmood_display_mode_t mode)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_display_mode = mode;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "切换显示模式: %d", mode);
}

carmood_display_mode_t carmood_ui_get_display_mode(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    carmood_display_mode_t ret = s_display_mode;
    xSemaphoreGive(s_mutex);
    return ret;
}

void carmood_ui_set_debug_mode(bool enabled)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_debug_mode = enabled;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "切换 DEBUG 模式: %d", enabled);
}

bool carmood_ui_is_debug_mode(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ret = s_debug_mode;
    xSemaphoreGive(s_mutex);
    return ret;
}

void carmood_ui_get_status(carmood_expr_t *expr,
                           char *turn_char,
                           char *pitch_char,
                           bool *muyu_mode,
                           int *muyu_count)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (expr) *expr = s_cur_expr;
    if (turn_char) *turn_char = s_turn_char;
    if (pitch_char) *pitch_char = s_pitch_char;
    if (muyu_mode) *muyu_mode = s_muyu_mode;
    if (muyu_count) *muyu_count = s_muyu_count;
    xSemaphoreGive(s_mutex);
}

void carmood_ui_enter_shooter(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    /* 特殊模式必须互斥，避免渲染层和输入层状态打架。 */
    s_muyu_mode      = false;
    s_muyu_animating = false;
    s_muyu_tapped    = false;
    s_shooter_mode = true;
    s_brick_mode   = false;
    s_flappy_mode  = false;
    xSemaphoreGive(s_mutex);
    shooter_game_init();
    ESP_LOGI(TAG, "进入打飞机游戏模式");
}

void carmood_ui_exit_shooter(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_shooter_mode = false;
    s_expr_changed = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "退出打飞机游戏模式");
}

bool carmood_ui_is_shooter_mode(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ret = s_shooter_mode;
    xSemaphoreGive(s_mutex);
    return ret;
}

void carmood_ui_enter_brick(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_muyu_mode      = false;
    s_muyu_animating = false;
    s_muyu_tapped    = false;
    s_shooter_mode   = false;
    s_brick_mode     = true;
    s_flappy_mode    = false;
    xSemaphoreGive(s_mutex);
    brick_game_init();
    ESP_LOGI(TAG, "进入打砖块模式");
}

void carmood_ui_exit_brick(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_brick_mode = false;
    s_expr_changed = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "退出打砖块模式");
}

bool carmood_ui_is_brick_mode(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ret = s_brick_mode;
    xSemaphoreGive(s_mutex);
    return ret;
}

void carmood_ui_brick_set_tilt(int32_t lr_val)
{
    brick_game_set_tilt(lr_val);
}

void carmood_ui_brick_step_left(void)
{
    brick_game_step_left();
}

void carmood_ui_brick_step_right(void)
{
    brick_game_step_right();
}

void carmood_ui_enter_flappy(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_muyu_mode      = false;
    s_muyu_animating = false;
    s_muyu_tapped    = false;
    s_shooter_mode   = false;
    s_brick_mode     = false;
    s_flappy_mode    = true;
    xSemaphoreGive(s_mutex);
    flappy_game_init();
    ESP_LOGI(TAG, "进入像素小鸟模式");
}

void carmood_ui_exit_flappy(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_flappy_mode = false;
    s_expr_changed = true;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "退出像素小鸟模式");
}

bool carmood_ui_is_flappy_mode(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool ret = s_flappy_mode;
    xSemaphoreGive(s_mutex);
    return ret;
}

void carmood_ui_flappy_jump(void)
{
    flappy_game_jump();
}

void carmood_ui_shooter_input_left(void)
{
    shooter_game_push_input(SHOOTER_INPUT_LEFT);
}

void carmood_ui_shooter_input_right(void)
{
    shooter_game_push_input(SHOOTER_INPUT_RIGHT);
}

void carmood_ui_shooter_input_fire(void)
{
    shooter_game_push_input(SHOOTER_INPUT_FIRE);
}

void carmood_ui_shooter_input_left_hold(void)
{
    shooter_game_push_input(SHOOTER_INPUT_LEFT_HOLD);
}

void carmood_ui_shooter_input_right_hold(void)
{
    shooter_game_push_input(SHOOTER_INPUT_RIGHT_HOLD);
}

void carmood_ui_shooter_input_release(void)
{
    shooter_game_push_input(SHOOTER_INPUT_RELEASE);
}
