#include "app/carmood_app.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "drivers/lis3dh.h"
#include "drivers/touch_input.h"
#include "ssd1306_oled.h"
#include "app/time_sync.h"
#include "ui/astra_menu_bridge.h"
#include "ui/carmood_ui.h"

static const char *TAG = "carmood";

#ifndef CONFIG_CARMOOD_TOUCH_SAMPLE_MS
#define CONFIG_CARMOOD_TOUCH_SAMPLE_MS 20
#endif
#ifndef CONFIG_CARMOOD_TOUCH_PRESS_THRESHOLD
#define CONFIG_CARMOOD_TOUCH_PRESS_THRESHOLD 300
#endif
#ifndef CONFIG_CARMOOD_TOUCH_RELEASE_THRESHOLD
#define CONFIG_CARMOOD_TOUCH_RELEASE_THRESHOLD 120
#endif
#ifndef CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT
#define CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT 3
#endif
#ifndef CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS
#define CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS 40
#endif
#ifndef CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS
#define CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS 800
#endif
#ifndef CONFIG_CARMOOD_TOUCH_MIN_GAP_MS
#define CONFIG_CARMOOD_TOUCH_MIN_GAP_MS 250
#endif
#ifndef CONFIG_CARMOOD_MENU_TIMEOUT_MS
#define CONFIG_CARMOOD_MENU_TIMEOUT_MS 10000
#endif
#ifndef CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS
#define CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS 300
#endif
#ifndef CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
#define CONFIG_CARMOOD_DEBUG_PERIODIC_LOG 0
#endif

typedef enum {
    APP_STATE_NORMAL = 0,
    APP_STATE_MENU,
} app_state_t;

typedef struct {
    bool wait_top_release_after_open;
} menu_state_t;

/* 触摸单击依次循环切换的表情列表（全部表情，方便调试预览） */
static const carmood_expr_t k_click_cycle[] = {
    EXPR_IDLE,
    EXPR_BLINK,
    EXPR_HAPPY,
    EXPR_SLEEPY,
    EXPR_TURN_LEFT,
    EXPR_TURN_RIGHT,
    EXPR_SURPRISED,
    EXPR_ANGRY,
};
#define CLICK_CYCLE_COUNT (int)(sizeof(k_click_cycle) / sizeof(k_click_cycle[0]))

static void carmood_handle_top_single_click(int *click_idx)
{
    if (carmood_ui_is_muyu_mode()) {
        carmood_ui_tap_muyu();
        ESP_LOGI(TAG, "敲木鱼！功德+1");
        return;
    }

    *click_idx = (*click_idx + 1) % CLICK_CYCLE_COUNT;
    carmood_ui_set_expression(k_click_cycle[*click_idx]);
    ESP_LOGI(TAG, "单击切换表情: %d", k_click_cycle[*click_idx]);
}

static void carmood_open_menu(app_state_t *app_state,
                              menu_state_t *menu_state,
                              int64_t *menu_last_action_ms,
                              int64_t now_ms,
                              const char *reason)
{
    carmood_ui_pause_animation();
    astra_menu_open();
    *app_state = APP_STATE_MENU;
    menu_state->wait_top_release_after_open = true;
    *menu_last_action_ms = now_ms;
    ESP_LOGI(TAG, "进入 Astra 菜单: %s", reason);
}

void carmood_app_run(void)
{
    ESP_LOGI(TAG, "CarMood 启动");

    esp_err_t ret = oled_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED 初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 启动动画后台任务 */
    carmood_ui_init();
    carmood_ui_set_expression(EXPR_IDLE);

    ret = carmood_time_sync_once();
    if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "时间同步失败，继续离线运行: %s", esp_err_to_name(ret));
    }

    ret = touch_input_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "触摸初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    astra_menu_init();

    ret = lis3dh_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LIS3DH 初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ret = lis3dh_rezero();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LIS3DH 初始归零失败: %s", esp_err_to_name(ret));
        return;
    }

    int     click_idx  = 0;
    app_state_t app_state = APP_STATE_NORMAL;
    menu_state_t menu_state = {0};
    int64_t menu_last_action_ms = 0;
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
    int64_t last_dir_log_ms = 0;
    int64_t last_steady_log_ms = 0;
    int64_t last_lis3dh_log_ms = 0;
#endif
    char    turn_char  = 'C';
    char    pitch_char = 'N';

    int64_t pending_top_click_ms = -1;

    while (1) {
        touch_input_event_t touch_event = {};
        ret = touch_input_poll(&touch_event);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "触摸采样失败: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        int64_t now_ms = esp_timer_get_time() / 1000;

        touch_input_key_event_t top_key = touch_event.keys[TOUCH_KEY_TOP];
        touch_input_key_event_t up_key = touch_event.keys[TOUCH_KEY_UP];
        touch_input_key_event_t down_key = touch_event.keys[TOUCH_KEY_DOWN];

        if (app_state == APP_STATE_MENU) {
            bool menu_input = false;

            if (menu_state.wait_top_release_after_open &&
                !touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
                menu_state.wait_top_release_after_open = false;
            }

            if (up_key.valid_click) {
                pending_top_click_ms = -1;
                astra_menu_input_up();
                menu_input = true;
            }
            if (down_key.valid_click) {
                pending_top_click_ms = -1;
                astra_menu_input_down();
                menu_input = true;
            }
            if (top_key.double_click) {
                pending_top_click_ms = -1;
                astra_menu_input_back();
                menu_input = true;
            } else if (top_key.valid_click && top_key.click_count == 1) {
                pending_top_click_ms = now_ms;
            }

            if (pending_top_click_ms >= 0 &&
                !top_key.released &&
                now_ms - pending_top_click_ms > CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS) {
                pending_top_click_ms = -1;
                astra_menu_input_ok();
                menu_input = true;
            }

            if (menu_input) {
                menu_last_action_ms = now_ms;
            }

            astra_menu_tick();

            if (!astra_menu_is_open()) {
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "退出 Astra 菜单: 顶部双击");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (astra_menu_should_toggle_muyu()) {
                astra_menu_consume_actions();
                astra_menu_close();
                if (carmood_ui_is_muyu_mode()) {
                    carmood_ui_exit_muyu();
                    carmood_ui_set_expression(k_click_cycle[click_idx]);
                } else {
                    carmood_ui_enter_muyu();
                }
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "菜单动作: 切换木鱼模式");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (astra_menu_should_run_calibration()) {
                astra_menu_consume_actions();
                astra_menu_close();
                app_state = APP_STATE_NORMAL;
                ESP_LOGI(TAG, "菜单动作: 方向标定");
                ret = lis3dh_run_guided_calibration(carmood_ui_show_calibration);
                if (ret != ESP_OK) {
                    ESP_LOGW(TAG, "方向标定失败: %s", esp_err_to_name(ret));
                }
                carmood_ui_set_expression(k_click_cycle[click_idx]);
                carmood_ui_resume_animation();
                menu_state.wait_top_release_after_open = false;
                pending_top_click_ms = -1;
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (now_ms - menu_last_action_ms >= CONFIG_CARMOOD_MENU_TIMEOUT_MS) {
                astra_menu_close();
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "退出 Astra 菜单: 超时");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        if (app_state == APP_STATE_NORMAL && top_key.double_click) {
            pending_top_click_ms = -1;
            carmood_open_menu(&app_state, &menu_state, &menu_last_action_ms, now_ms, "顶部双击");
            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        if (app_state == APP_STATE_NORMAL && top_key.valid_click && top_key.click_count == 1) {
            pending_top_click_ms = now_ms;
        }

        if (app_state == APP_STATE_NORMAL &&
            pending_top_click_ms >= 0 &&
            !top_key.released &&
            now_ms - pending_top_click_ms > CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS) {
            carmood_handle_top_single_click(&click_idx);
            pending_top_click_ms = -1;
        }

#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
        if (now_ms - last_steady_log_ms >= 1000) {
            ESP_LOGI(TAG, "touch: bm=%lu sm=%lu delta=%ld state=%s",
                     (unsigned long)touch_event.samples[TOUCH_KEY_TOP].benchmark,
                     (unsigned long)touch_event.samples[TOUCH_KEY_TOP].smooth,
                     (long)touch_event.samples[TOUCH_KEY_TOP].delta,
                     touch_event.samples[TOUCH_KEY_TOP].stable_pressed ? "PRESSED" : "RELEASED");
            last_steady_log_ms = now_ms;
        }
#endif

        /* LIS3DH 读取与方向判断 */
        if (
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
            now_ms - last_lis3dh_log_ms >= 500
#else
            true
#endif
        ) {
            int16_t ax = 0, ay = 0, az = 0;
            ret = lis3dh_read_raw(&ax, &ay, &az);
            if (ret == ESP_OK && lis3dh_is_zero_ready()) {
                int32_t dx = 0, dy = 0, dz = 0;
                int32_t lr_val = 0, fb_val = 0;
                char    new_turn = 'C', new_pitch = 'N';

                lis3dh_get_delta(ax, ay, az, &dx, &dy, &dz);
                lis3dh_eval_direction(dx, dy, dz, &new_turn, &new_pitch,
                                      &lr_val, &fb_val);

                bool dir_changed = (new_turn != turn_char) || (new_pitch != pitch_char);
                if (dir_changed) {
                    turn_char  = new_turn;
                    pitch_char = new_pitch;
                    carmood_ui_set_direction_overlay(turn_char, pitch_char);

                    /* 转弯时自动切换对应动画 */
                    if (turn_char == 'L') {
                        carmood_ui_set_expression(EXPR_TURN_LEFT);
                    } else if (turn_char == 'R') {
                        carmood_ui_set_expression(EXPR_TURN_RIGHT);
                    } else {
                        /* 回正时恢复触摸选中的表情 */
                        carmood_ui_set_expression(k_click_cycle[click_idx]);
                    }

                    ESP_LOGI(TAG, "方向变化: %c/%c (lr=%ld fb=%ld)",
                             turn_char, pitch_char, (long)lr_val, (long)fb_val);
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
                    last_dir_log_ms = now_ms;
#endif
                } else if (
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
                    now_ms - last_dir_log_ms >= 2000
#else
                    false
#endif
                ) {
                    ESP_LOGI(TAG, "方向稳定: %c/%c (lr=%ld fb=%ld)",
                             turn_char, pitch_char, (long)lr_val, (long)fb_val);
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
                    last_dir_log_ms = now_ms;
#endif
                }

#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
                ESP_LOGI(TAG, "LIS3DH raw=(%d,%d,%d) delta=(%ld,%ld,%ld)",
                         ax, ay, az, (long)dx, (long)dy, (long)dz);
#endif
            } else if (ret == ESP_OK) {
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
                ESP_LOGI(TAG, "LIS3DH xyz(raw): x=%d y=%d z=%d", ax, ay, az);
#endif
            } else {
                ESP_LOGW(TAG, "读取 LIS3DH xyz 失败: %s", esp_err_to_name(ret));
            }
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
            last_lis3dh_log_ms = now_ms;
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
    }
}
