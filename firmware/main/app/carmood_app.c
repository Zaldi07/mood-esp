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
#include "ui/astra_lite/astra_ui_core.h"
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
#ifndef CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS
#define CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS 500
#endif
#ifndef CONFIG_CARMOOD_MENU_BACK_HOLD_MS
#define CONFIG_CARMOOD_MENU_BACK_HOLD_MS 500
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
    bool top_hold_handled;
    bool wait_top_release_after_close;
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

    if (carmood_ui_get_display_mode() == CARMOOD_DISPLAY_MODE_CLOCK) {
        ESP_LOGI(TAG, "时钟模式下忽略顶部单击");
        return;
    }

    *click_idx = (*click_idx + 1) % CLICK_CYCLE_COUNT;
    carmood_ui_set_expression(k_click_cycle[*click_idx]);
    ESP_LOGI(TAG, "单击切换表情: %d", k_click_cycle[*click_idx]);
}

static void carmood_open_menu(app_state_t *app_state,
                              menu_state_t *menu_state,
                              const char *reason)
{
    carmood_ui_pause_animation();
    astra_menu_open();
    *app_state = APP_STATE_MENU;
    menu_state->wait_top_release_after_open = true;
    ESP_LOGI(TAG, "进入 Astra 菜单: %s", reason);
}

static bool carmood_read_lis3dh_direction(char *turn_state,
                                          char *pitch_state,
                                          int32_t *lr_val,
                                          int32_t *fb_val)
{
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    esp_err_t ret = lis3dh_read_raw(&ax, &ay, &az);
    if (ret != ESP_OK || !lis3dh_is_zero_ready()) {
        return false;
    }

    int32_t dx = 0;
    int32_t dy = 0;
    int32_t dz = 0;
    lis3dh_get_delta(ax, ay, az, &dx, &dy, &dz);
    lis3dh_eval_direction(dx, dy, dz, turn_state, pitch_state, lr_val, fb_val);
    return true;
}

static carmood_display_mode_t carmood_next_display_mode(carmood_display_mode_t mode)
{
    switch (mode) {
        case CARMOOD_DISPLAY_MODE_FACE:
            return CARMOOD_DISPLAY_MODE_CLOCK;
        case CARMOOD_DISPLAY_MODE_CLOCK:
        default:
            return CARMOOD_DISPLAY_MODE_FACE;
    }
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
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
    int64_t last_dir_log_ms = 0;
    int64_t last_steady_log_ms = 0;
    int64_t last_lis3dh_log_ms = 0;
#endif
    char    turn_char  = 'C';
    char    pitch_char = 'N';

    int64_t pending_top_click_ms = -1;
    bool flappy_top_was_pressed = false;
    int32_t prev_dx = 0;
    int32_t prev_dy = 0;
    int32_t prev_dz = 0;

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
        bool in_muyu_mode = carmood_ui_is_muyu_mode();

        if (app_state == APP_STATE_MENU) {
            bool in_user_item = astra_is_in_user_item();

            if (menu_state.wait_top_release_after_open &&
                !touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
                menu_state.wait_top_release_after_open = false;
            }

            if (!touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
                menu_state.top_hold_handled = false;
            }

            if (in_user_item) {
                pending_top_click_ms = -1;
            }

            if (up_key.valid_click || touch_event.swipe == TOUCH_SWIPE_UP) {
                pending_top_click_ms = -1;
                astra_menu_input_up();
            }
            if (down_key.valid_click || touch_event.swipe == TOUCH_SWIPE_DOWN) {
                astra_menu_input_down();
            }
            if (!menu_state.top_hold_handled &&
                !menu_state.wait_top_release_after_open &&
                top_key.valid_click && top_key.click_count == 1) {
                pending_top_click_ms = -1;
                astra_menu_input_ok();
            }

            if (!menu_state.top_hold_handled &&
                !menu_state.wait_top_release_after_open &&
                touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
                top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
                menu_state.top_hold_handled = true;
                pending_top_click_ms = -1;
                astra_menu_input_back();
            }

            astra_menu_tick();

            if (!astra_menu_is_open()) {
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                menu_state.top_hold_handled = false;
                menu_state.wait_top_release_after_close = true;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "退出 Astra 菜单: 顶部长按");
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
                menu_state.top_hold_handled = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "菜单动作: 切换木鱼模式");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (astra_menu_should_toggle_shooter()) {
                astra_menu_consume_actions();
                astra_menu_close();
                if (carmood_ui_is_shooter_mode()) {
                    carmood_ui_exit_shooter();
                    carmood_ui_set_expression(k_click_cycle[click_idx]);
                } else {
                    carmood_ui_enter_shooter();
                }
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                menu_state.top_hold_handled = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "菜单动作: 切换打飞机游戏");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (astra_menu_should_toggle_brick()) {
                astra_menu_consume_actions();
                astra_menu_close();
                if (carmood_ui_is_brick_mode()) {
                    carmood_ui_exit_brick();
                    carmood_ui_set_expression(k_click_cycle[click_idx]);
                } else {
                    carmood_ui_enter_brick();
                }
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                menu_state.top_hold_handled = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "菜单动作: 切换打砖块游戏");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (astra_menu_should_toggle_flappy()) {
                astra_menu_consume_actions();
                astra_menu_close();
                if (carmood_ui_is_flappy_mode()) {
                    carmood_ui_exit_flappy();
                    carmood_ui_set_expression(k_click_cycle[click_idx]);
                } else {
                    carmood_ui_enter_flappy();
                }
                carmood_ui_resume_animation();
                app_state = APP_STATE_NORMAL;
                menu_state.wait_top_release_after_open = false;
                menu_state.top_hold_handled = false;
                pending_top_click_ms = -1;
                ESP_LOGI(TAG, "菜单动作: 切换像素小鸟");
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
                menu_state.top_hold_handled = false;
                pending_top_click_ms = -1;
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        /* 打飞机游戏模式输入处理 */
        bool in_shooter_mode = carmood_ui_is_shooter_mode();
        if (app_state == APP_STATE_NORMAL && in_shooter_mode) {
            if (touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
                top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
                pending_top_click_ms = -1;
                carmood_ui_exit_shooter();
                carmood_ui_set_expression(k_click_cycle[click_idx]);
                ESP_LOGI(TAG, "顶部长按退出打飞机游戏");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }
            /* 上键持续按住=持续左移，下键持续按住=持续右移 */
            bool up_held   = touch_event.samples[TOUCH_KEY_UP].stable_pressed;
            bool down_held = touch_event.samples[TOUCH_KEY_DOWN].stable_pressed;
            if (up_held && !down_held) {
                carmood_ui_shooter_input_left_hold();
            } else if (down_held && !up_held) {
                carmood_ui_shooter_input_right_hold();
            } else if (!up_held && !down_held) {
                char tilt_turn = turn_char;
                char tilt_pitch = pitch_char;
                int32_t lr_val = 0;
                int32_t fb_val = 0;
                if (carmood_read_lis3dh_direction(&tilt_turn, &tilt_pitch,
                                                  &lr_val, &fb_val)) {
                    bool dir_changed =
                        (tilt_turn != turn_char) || (tilt_pitch != pitch_char);
                    turn_char = tilt_turn;
                    pitch_char = tilt_pitch;
                    carmood_ui_set_direction_overlay(turn_char, pitch_char);
                    if (dir_changed) {
                        ESP_LOGI(TAG, "游戏方向: %c/%c (lr=%ld fb=%ld)",
                                 turn_char, pitch_char,
                                 (long)lr_val, (long)fb_val);
                    }

                    if (lr_val <= -600) {
                        carmood_ui_shooter_input_left_hold();
                    } else if (lr_val >= 600) {
                        carmood_ui_shooter_input_right_hold();
                    } else {
                        carmood_ui_shooter_input_release();
                    }
                } else {
                    carmood_ui_shooter_input_release();
                }
            }
            /* 滑动也可以移动（快速拨一下） */
            if (touch_event.swipe == TOUCH_SWIPE_UP) {
                carmood_ui_shooter_input_left();
            } else if (touch_event.swipe == TOUCH_SWIPE_DOWN) {
                carmood_ui_shooter_input_right();
            }
            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        bool in_brick_mode = carmood_ui_is_brick_mode();
        if (app_state == APP_STATE_NORMAL && in_brick_mode) {
            if (touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
                top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
                pending_top_click_ms = -1;
                carmood_ui_exit_brick();
                carmood_ui_set_expression(k_click_cycle[click_idx]);
                ESP_LOGI(TAG, "顶部长按退出打砖块");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            bool up_held   = touch_event.samples[TOUCH_KEY_UP].stable_pressed;
            bool down_held = touch_event.samples[TOUCH_KEY_DOWN].stable_pressed;
            if (up_held && !down_held) {
                carmood_ui_brick_set_tilt(-1400);
            } else if (down_held && !up_held) {
                carmood_ui_brick_set_tilt(1400);
            } else if (up_key.valid_click || touch_event.swipe == TOUCH_SWIPE_UP) {
                carmood_ui_brick_step_left();
                carmood_ui_brick_set_tilt(0);
            } else if (down_key.valid_click || touch_event.swipe == TOUCH_SWIPE_DOWN) {
                carmood_ui_brick_step_right();
                carmood_ui_brick_set_tilt(0);
            } else {
            int32_t lr_val = 0;
            int32_t fb_val = 0;
            char tilt_turn = turn_char;
            char tilt_pitch = pitch_char;
            if (carmood_read_lis3dh_direction(&tilt_turn, &tilt_pitch,
                                              &lr_val, &fb_val)) {
                turn_char = tilt_turn;
                pitch_char = tilt_pitch;
                carmood_ui_set_direction_overlay(turn_char, pitch_char);
                carmood_ui_brick_set_tilt(lr_val);
            } else {
                carmood_ui_brick_set_tilt(0);
            }
            }

            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        bool in_flappy_mode = carmood_ui_is_flappy_mode();
        if (app_state == APP_STATE_NORMAL && in_flappy_mode) {
            bool top_pressed = touch_event.samples[TOUCH_KEY_TOP].stable_pressed;

            if (touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
                top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
                pending_top_click_ms = -1;
                flappy_top_was_pressed = false;
                carmood_ui_exit_flappy();
                carmood_ui_set_expression(k_click_cycle[click_idx]);
                ESP_LOGI(TAG, "顶部长按退出像素小鸟");
                vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
                continue;
            }

            if (top_pressed && !flappy_top_was_pressed) {
                pending_top_click_ms = -1;
                carmood_ui_flappy_jump();
            }

            flappy_top_was_pressed = top_pressed;

            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        flappy_top_was_pressed = false;

        /* 右侧滑动切换显示模式（表情 ↔ 时钟） */
        if (app_state == APP_STATE_NORMAL && !in_muyu_mode &&
            !in_flappy_mode &&
            touch_event.swipe != TOUCH_SWIPE_NONE) {
            pending_top_click_ms = -1;
            carmood_display_mode_t cur_mode = carmood_ui_get_display_mode();
            carmood_display_mode_t new_mode = carmood_next_display_mode(cur_mode);
            carmood_ui_set_display_mode(new_mode);
            ESP_LOGI(TAG, "滑动切换模式: %s",
                     new_mode == CARMOOD_DISPLAY_MODE_FACE ? "表情" :
                     (new_mode == CARMOOD_DISPLAY_MODE_CLOCK ? "时钟" : "布局"));
            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        if (menu_state.wait_top_release_after_close &&
            !touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
            menu_state.wait_top_release_after_close = false;
        }

        if (app_state == APP_STATE_NORMAL &&
            in_muyu_mode &&
            !menu_state.wait_top_release_after_close &&
            touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
            top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
            pending_top_click_ms = -1;
            carmood_ui_exit_muyu();
            carmood_ui_set_expression(k_click_cycle[click_idx]);
            ESP_LOGI(TAG, "顶部长按退出功德木鱼");
            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        if (app_state == APP_STATE_NORMAL && !in_muyu_mode &&
            !in_flappy_mode && top_key.double_click) {
            pending_top_click_ms = -1;
            carmood_open_menu(&app_state, &menu_state, "顶部双击");
            vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
            continue;
        }

        if (app_state == APP_STATE_NORMAL && in_muyu_mode && top_key.valid_click) {
            pending_top_click_ms = -1;
            carmood_handle_top_single_click(&click_idx);
        }

        if (app_state == APP_STATE_NORMAL &&
            !in_muyu_mode &&
            !in_flappy_mode &&
            top_key.valid_click &&
            top_key.click_count == 1) {
            pending_top_click_ms = now_ms;
        }

        if (app_state == APP_STATE_NORMAL &&
            !in_muyu_mode &&
            !in_flappy_mode &&
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

                int32_t ddx = dx - prev_dx;
                int32_t ddy = dy - prev_dy;
                int32_t ddz = dz - prev_dz;
                int32_t shake_val = (ddx < 0 ? -ddx : ddx) +
                                    (ddy < 0 ? -ddy : ddy) +
                                    (ddz < 0 ? -ddz : ddz);
                prev_dx = dx;
                prev_dy = dy;
                prev_dz = dz;
                carmood_ui_set_motion_input(lr_val, fb_val, shake_val);

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
                carmood_ui_set_motion_input(0, 0, 0);
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
                ESP_LOGI(TAG, "LIS3DH xyz(raw): x=%d y=%d z=%d", ax, ay, az);
#endif
            } else {
                carmood_ui_set_motion_input(0, 0, 0);
                ESP_LOGW(TAG, "读取 LIS3DH xyz 失败: %s", esp_err_to_name(ret));
            }
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
            last_lis3dh_log_ms = now_ms;
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
    }
}
