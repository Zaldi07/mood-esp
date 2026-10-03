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
#include "drivers/buzzer.h"
#include "ssd1306_oled.h"
#include "app/carmood_face_controller.h"
#include "app/time_sync.h"
#include "ui/astra_menu_bridge.h"
#include "ui/astra_lite/astra_ui_core.h"
#include "ui/carmood_ui.h"
#include "ui/boot_intro.h"
#include "ui/boot_quote.h"
#include "app/power_sleep.h"

static const char* TAG = "carmood";

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
#define CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS 15
#endif
#ifndef CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS
#define CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS 400
#endif
#ifndef CONFIG_CARMOOD_TOUCH_MIN_GAP_MS
#define CONFIG_CARMOOD_TOUCH_MIN_GAP_MS 250
#endif
#ifndef CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS
#define CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS 380
#endif
#ifndef CONFIG_CARMOOD_MENU_BACK_HOLD_MS
#define CONFIG_CARMOOD_MENU_BACK_HOLD_MS 450
#endif
#ifndef CONFIG_CARMOOD_GAME_EXIT_HOLD_MS
#define CONFIG_CARMOOD_GAME_EXIT_HOLD_MS 1500
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

static void carmood_handle_top_single_click(carmood_face_controller_t* face_state, int64_t now_ms) {
  if (carmood_ui_is_muyu_mode()) {
    carmood_ui_tap_muyu();
    ESP_LOGI(TAG, "敲木鱼！功德+1");
    return;
  }

  if (carmood_ui_get_display_mode() == CARMOOD_DISPLAY_MODE_CLOCK) {
    carmood_ui_set_display_mode(CARMOOD_DISPLAY_MODE_FACE);
    carmood_face_restore_idle(face_state, now_ms);
    ESP_LOGI(TAG, "Clock mode: single click to return to Face mode");
    return;
  }

  carmood_face_trigger_interaction(face_state, now_ms);
  ESP_LOGI(TAG, "顶部单击触发桌宠互动");
}

static void carmood_open_menu(app_state_t* app_state, menu_state_t* menu_state,
                              const char* reason) {
  carmood_ui_pause_animation();
  astra_menu_open();
  *app_state = APP_STATE_MENU;
  menu_state->wait_top_release_after_open = true;
  ESP_LOGI(TAG, "进入 Astra 菜单: %s", reason);
}

static bool s_has_lis3dh = false;

static bool carmood_read_lis3dh_direction(char* turn_state, char* pitch_state, int32_t* lr_val,
                                          int32_t* fb_val) {
  if (!s_has_lis3dh) {
    *turn_state = 'C';
    *pitch_state = 'N';
    *lr_val = 0;
    *fb_val = 0;
    return false;
  }
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

static carmood_display_mode_t carmood_next_display_mode(carmood_display_mode_t mode) {
  switch (mode) {
    case CARMOOD_DISPLAY_MODE_FACE:
      return CARMOOD_DISPLAY_MODE_CLOCK;
    case CARMOOD_DISPLAY_MODE_CLOCK:
    default:
      return CARMOOD_DISPLAY_MODE_FACE;
  }
}

static void carmood_set_persona(carmood_face_controller_t* face_state,
                                carmood_pet_persona_t persona, int64_t now_ms) {
  carmood_face_set_persona(face_state, persona, now_ms);
  ESP_LOGI(TAG, "切换桌宠人格: %d", (int)persona);
}

void carmood_app_run(void) {
  ESP_LOGI(TAG, "CarMood 启动");

  esp_err_t ret = oled_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "OLED 初始化失败: %s", esp_err_to_name(ret));
    return;
  }

  ret = touch_input_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "触摸初始化失败: %s", esp_err_to_name(ret));
    return;
  }

  buzzer_init();
  buzzer_play_intro_melody_async();

  /* 1. 开机播放 intro.gif 动画 (伴随 chiptune 蜂鸣器旋律) */
  boot_intro_run();

  /* 2. 动画结束后播放 Babel Quotes 引言文字动画 */
  boot_quote_run_intro();

  /* 启动动画后台任务 */
  carmood_ui_init();

  ret = carmood_time_sync_once();
  if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "时间同步失败，继续离线运行: %s", esp_err_to_name(ret));
  }

  astra_menu_init();

  ret = lis3dh_init();
  if (ret == ESP_OK) {
    ret = lis3dh_rezero();
    if (ret == ESP_OK) {
      s_has_lis3dh = true;
      ESP_LOGI(TAG, "LIS3DH 初始化并归零成功");
    } else {
      ESP_LOGW(TAG, "LIS3DH 初始归零失败: %s，已跳过加速度计", esp_err_to_name(ret));
    }
  } else {
    ESP_LOGW(TAG, "未检测到 LIS3DH 加速度计，已跳过（仅运行屏幕与触摸）");
  }

  app_state_t app_state = APP_STATE_NORMAL;
  menu_state_t menu_state = {0};
  int64_t face_init_ms = esp_timer_get_time() / 1000;
  carmood_face_controller_t face_state;
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
  int64_t last_dir_log_ms = 0;
  int64_t last_steady_log_ms = 0;
  int64_t last_lis3dh_log_ms = 0;
#endif
  char turn_char = 'C';
  char pitch_char = 'N';

  bool flappy_top_was_pressed = false;
  bool wait_game_top_release = false;
  int32_t prev_dx = 0;
  int32_t prev_dy = 0;
  int32_t prev_dz = 0;
  int32_t prev_lr_val = 0;
  int32_t prev_fb_val = 0;

  carmood_face_init(&face_state, face_init_ms, (uint32_t)esp_timer_get_time());
  bool was_showing_sleep_countdown = false;

  while (1) {
    touch_input_event_t touch_event = {};
    ret = touch_input_poll(&touch_event);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "触摸采样失败: %s", esp_err_to_name(ret));
      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;

    if (wait_game_top_release) {
      if (!touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
        wait_game_top_release = false;
      }
    }

    touch_input_key_event_t top_key = touch_event.keys[TOUCH_KEY_TOP];
    touch_input_key_event_t up_key = touch_event.keys[TOUCH_KEY_UP];
    touch_input_key_event_t down_key = touch_event.keys[TOUCH_KEY_DOWN];
    bool has_touch_activity = touch_event.swipe != TOUCH_SWIPE_NONE ||
                              touch_event.samples[TOUCH_KEY_TOP].stable_pressed ||
                              touch_event.samples[TOUCH_KEY_UP].stable_pressed ||
                              touch_event.samples[TOUCH_KEY_DOWN].stable_pressed ||
                              top_key.released || up_key.released || down_key.released;
    bool in_muyu_mode = carmood_ui_is_muyu_mode();

    if (has_touch_activity) {
      carmood_ui_notify_activity();
    }

    /* 5-second long-press on Touch Sensor (GPIO 7) triggers Sleep Mode */
    if (touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
        top_key.press_ms >= POWER_SLEEP_HOLD_TRIGGER_MS) {
      power_sleep_enter();
      break;
    }

    if (touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
        top_key.press_ms >= POWER_SLEEP_HOLD_THRESHOLD_MS) {
      was_showing_sleep_countdown = true;
      power_sleep_draw_countdown(top_key.press_ms);
      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    if (was_showing_sleep_countdown && !touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
      was_showing_sleep_countdown = false;
      carmood_ui_resume_animation();
    }

    if (app_state == APP_STATE_MENU) {
      if (menu_state.wait_top_release_after_open &&
          !touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
        menu_state.wait_top_release_after_open = false;
      }

      if (!touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
        menu_state.top_hold_handled = false;
      }

      if (astra_menu_is_in_user_page()) {
        if (top_key.valid_click || (touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
                                    top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS)) {
          astra_menu_input_back();
          ESP_LOGI(TAG, "Exiting user status page");
          vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
          continue;
        }
      }

      if (up_key.valid_click || touch_event.swipe == TOUCH_SWIPE_UP) {
        astra_menu_input_up();
      }
      if (down_key.valid_click || touch_event.swipe == TOUCH_SWIPE_DOWN ||
          (top_key.valid_click && !top_key.double_click && !menu_state.top_hold_handled &&
           !menu_state.wait_top_release_after_open)) {
        astra_menu_input_down();
        ESP_LOGI(TAG, "菜单动作: 下移(单击)");
      }
      if (top_key.double_click) {
        astra_menu_input_back();
        ESP_LOGI(TAG, "菜单动作: 返回(双击)");
      }
      if (!menu_state.top_hold_handled && !menu_state.wait_top_release_after_open &&
          touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
          top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
        menu_state.top_hold_handled = true;
        astra_menu_input_ok();
        ESP_LOGI(TAG, "菜单动作: 确认(长按)");
      }

      astra_menu_tick();

      if (!astra_menu_is_open()) {
        carmood_ui_resume_animation();
        app_state = APP_STATE_NORMAL;
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        menu_state.wait_top_release_after_close = true;
        ESP_LOGI(TAG, "退出 Astra 菜单: 顶部长按");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (astra_menu_should_toggle_muyu()) {
        astra_menu_consume_actions();
        astra_menu_close();
        if (carmood_ui_is_muyu_mode()) {
          carmood_ui_exit_muyu();
          carmood_face_restore_idle(&face_state, now_ms);
        } else {
          carmood_ui_enter_muyu();
        }
        carmood_ui_resume_animation();
        app_state = APP_STATE_NORMAL;
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        wait_game_top_release = true;
        ESP_LOGI(TAG, "菜单动作: 切换木鱼模式");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (astra_menu_should_toggle_shooter()) {
        astra_menu_consume_actions();
        astra_menu_close();
        if (carmood_ui_is_shooter_mode()) {
          carmood_ui_exit_shooter();
          carmood_face_restore_idle(&face_state, now_ms);
        } else {
          carmood_ui_enter_shooter();
        }
        carmood_ui_resume_animation();
        app_state = APP_STATE_NORMAL;
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        wait_game_top_release = true;
        ESP_LOGI(TAG, "菜单动作: 切换打飞机游戏");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (astra_menu_should_toggle_brick()) {
        astra_menu_consume_actions();
        astra_menu_close();
        if (carmood_ui_is_brick_mode()) {
          carmood_ui_exit_brick();
          carmood_face_restore_idle(&face_state, now_ms);
        } else {
          carmood_ui_enter_brick();
        }
        carmood_ui_resume_animation();
        app_state = APP_STATE_NORMAL;
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        wait_game_top_release = true;
        ESP_LOGI(TAG, "菜单动作: 切换打砖块游戏");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (astra_menu_should_toggle_flappy()) {
        astra_menu_consume_actions();
        astra_menu_close();
        if (carmood_ui_is_flappy_mode()) {
          carmood_ui_exit_flappy();
          carmood_face_restore_idle(&face_state, now_ms);
        } else {
          carmood_ui_enter_flappy();
        }
        carmood_ui_resume_animation();
        app_state = APP_STATE_NORMAL;
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        wait_game_top_release = true;
        ESP_LOGI(TAG, "菜单动作: 切换像素小鸟");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (astra_menu_should_toggle_pomodoro()) {
        astra_menu_consume_actions();
        astra_menu_close();
        if (carmood_ui_is_pomodoro_mode()) {
          carmood_ui_exit_pomodoro();
          carmood_face_restore_idle(&face_state, now_ms);
        } else {
          carmood_ui_enter_pomodoro();
        }
        carmood_ui_resume_animation();
        app_state = APP_STATE_NORMAL;
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        wait_game_top_release = true;
        ESP_LOGI(TAG, "菜单动作: 切换番茄钟模式 (Pomodoro)");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (astra_menu_should_run_calibration()) {
        astra_menu_consume_actions();
        astra_menu_close();
        app_state = APP_STATE_NORMAL;
        ESP_LOGI(TAG, "菜单动作: 方向标定");
        if (s_has_lis3dh) {
          ret = lis3dh_run_guided_calibration(carmood_ui_show_calibration);
          if (ret != ESP_OK) {
            ESP_LOGW(TAG, "方向标定失败: %s", esp_err_to_name(ret));
          }
        } else {
          ESP_LOGW(TAG, "未安装 LIS3DH，跳过方向标定");
        }
        carmood_face_restore_idle(&face_state, now_ms);
        carmood_ui_resume_animation();
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        menu_state.wait_top_release_after_close = true;
        wait_game_top_release = true;
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      astra_pet_persona_action_t persona_action = astra_menu_get_pet_persona_action();
      if (persona_action != ASTRA_PET_PERSONA_NONE) {
        astra_menu_consume_actions();
        astra_menu_close();
        app_state = APP_STATE_NORMAL;
        switch (persona_action) {
          case ASTRA_PET_PERSONA_PLAYFUL:
            carmood_set_persona(&face_state, CARMOOD_PET_PERSONA_PLAYFUL, now_ms);
            break;
          case ASTRA_PET_PERSONA_SLEEPY:
            carmood_set_persona(&face_state, CARMOOD_PET_PERSONA_SLEEPY, now_ms);
            break;
          case ASTRA_PET_PERSONA_TSUNDERE:
            carmood_set_persona(&face_state, CARMOOD_PET_PERSONA_TSUNDERE, now_ms);
            break;
          case ASTRA_PET_PERSONA_CURIOUS:
            carmood_set_persona(&face_state, CARMOOD_PET_PERSONA_CURIOUS, now_ms);
            break;
          case ASTRA_PET_PERSONA_COOL:
            carmood_set_persona(&face_state, CARMOOD_PET_PERSONA_COOL, now_ms);
            break;
          case ASTRA_PET_PERSONA_DEFAULT:
          default:
            carmood_set_persona(&face_state, CARMOOD_PET_PERSONA_DEFAULT, now_ms);
            break;
        }
        carmood_ui_resume_animation();
        menu_state.wait_top_release_after_open = false;
        menu_state.top_hold_handled = false;
        menu_state.wait_top_release_after_close = true;
        wait_game_top_release = true;
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    /* 打飞机游戏模式输入处理 */
    bool in_shooter_mode = carmood_ui_is_shooter_mode();
    if (app_state == APP_STATE_NORMAL && in_shooter_mode) {
      if (!wait_game_top_release && touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
          top_key.press_ms >= CONFIG_CARMOOD_GAME_EXIT_HOLD_MS) {
        carmood_ui_exit_shooter();
        carmood_face_restore_idle(&face_state, now_ms);
        wait_game_top_release = true;
        ESP_LOGI(TAG, "顶部长按退出打飞机游戏");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }
      /* 上键持续按住=持续左移，下键持续按住=持续右移 */
      bool up_held = touch_event.samples[TOUCH_KEY_UP].stable_pressed;
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
        if (carmood_read_lis3dh_direction(&tilt_turn, &tilt_pitch, &lr_val, &fb_val)) {
          bool dir_changed = (tilt_turn != turn_char) || (tilt_pitch != pitch_char);
          turn_char = tilt_turn;
          pitch_char = tilt_pitch;
          carmood_ui_set_direction_overlay(turn_char, pitch_char);
          if (dir_changed) {
            ESP_LOGI(TAG, "游戏方向: %c/%c (lr=%ld fb=%ld)", turn_char, pitch_char, (long)lr_val,
                     (long)fb_val);
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
      if (!wait_game_top_release && touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
          top_key.press_ms >= CONFIG_CARMOOD_GAME_EXIT_HOLD_MS) {
        carmood_ui_exit_brick();
        carmood_face_restore_idle(&face_state, now_ms);
        wait_game_top_release = true;
        ESP_LOGI(TAG, "顶部长按退出打砖块");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      bool up_held = touch_event.samples[TOUCH_KEY_UP].stable_pressed;
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
        if (carmood_read_lis3dh_direction(&tilt_turn, &tilt_pitch, &lr_val, &fb_val)) {
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

      if (!wait_game_top_release && touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
          top_key.press_ms >= CONFIG_CARMOOD_GAME_EXIT_HOLD_MS) {
        flappy_top_was_pressed = false;
        carmood_ui_exit_flappy();
        carmood_face_restore_idle(&face_state, now_ms);
        wait_game_top_release = true;
        ESP_LOGI(TAG, "顶部长按退出像素小鸟");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      if (!wait_game_top_release && top_pressed && !flappy_top_was_pressed) {
        carmood_ui_flappy_jump();
      }

      flappy_top_was_pressed = top_pressed;

      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    flappy_top_was_pressed = false;

    bool in_pomodoro_mode = carmood_ui_is_pomodoro_mode();
    if (app_state == APP_STATE_NORMAL && in_pomodoro_mode) {
      /* Exit Pomodoro with long press (>= 1.5s) */
      if (!wait_game_top_release && touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
          top_key.press_ms >= CONFIG_CARMOOD_GAME_EXIT_HOLD_MS) {
        carmood_ui_exit_pomodoro();
        carmood_face_restore_idle(&face_state, now_ms);
        wait_game_top_release = true;
        ESP_LOGI(TAG, "顶部长按退出番茄钟");
        vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
        continue;
      }

      /* Single Tap: Start / Pause / Next session */
      if (!wait_game_top_release && top_key.valid_click && !top_key.double_click) {
        carmood_ui_pomodoro_tap();
      }

      /* Double Tap: Reset / switch */
      if (!wait_game_top_release && top_key.double_click) {
        carmood_ui_pomodoro_double_tap();
      }

      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    /* 右侧两个键单按切换显示模式（表情 ↔ 时钟） */
    if (app_state == APP_STATE_NORMAL && !in_muyu_mode && !in_flappy_mode && !in_brick_mode &&
        !in_shooter_mode && !in_pomodoro_mode && (up_key.valid_click || down_key.valid_click)) {
      carmood_display_mode_t cur_mode = carmood_ui_get_display_mode();
      carmood_display_mode_t new_mode = carmood_next_display_mode(cur_mode);
      carmood_ui_set_display_mode(new_mode);
      ESP_LOGI(TAG, "右侧单击切换模式: %s",
               new_mode == CARMOOD_DISPLAY_MODE_FACE
                   ? "表情"
                   : (new_mode == CARMOOD_DISPLAY_MODE_CLOCK ? "时钟" : "布局"));
      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    if (menu_state.wait_top_release_after_close &&
        !touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
      menu_state.wait_top_release_after_close = false;
    }

    if (app_state == APP_STATE_NORMAL && in_muyu_mode && !wait_game_top_release &&
        !menu_state.wait_top_release_after_close &&
        touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
        top_key.press_ms >= CONFIG_CARMOOD_GAME_EXIT_HOLD_MS) {
      carmood_ui_exit_muyu();
      carmood_face_restore_idle(&face_state, now_ms);
      wait_game_top_release = true;
      ESP_LOGI(TAG, "顶部长按退出功德木鱼");
      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    if (app_state == APP_STATE_NORMAL && !in_muyu_mode && !in_flappy_mode && !in_brick_mode &&
        !in_shooter_mode && !in_pomodoro_mode && !wait_game_top_release &&
        !menu_state.wait_top_release_after_close &&
        touch_event.samples[TOUCH_KEY_TOP].stable_pressed &&
        top_key.press_ms >= CONFIG_CARMOOD_MENU_BACK_HOLD_MS) {
      carmood_open_menu(&app_state, &menu_state, "Long Press");
      vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
      continue;
    }

    if (app_state == APP_STATE_NORMAL && in_muyu_mode && !wait_game_top_release &&
        top_key.valid_click) {
      carmood_handle_top_single_click(&face_state, now_ms);
    }

    if (app_state == APP_STATE_NORMAL && !in_muyu_mode && !in_flappy_mode && !in_brick_mode &&
        !in_shooter_mode && !in_pomodoro_mode && !wait_game_top_release && top_key.valid_click) {
      if (carmood_ui_get_display_mode() == CARMOOD_DISPLAY_MODE_CLOCK) {
        /* In Clock mode: tap immediately exits back to Face mode! */
        carmood_ui_set_display_mode(CARMOOD_DISPLAY_MODE_FACE);
        carmood_face_restore_idle(&face_state, now_ms);
        ESP_LOGI(TAG, "Clock mode: tap -> exit to face mode");
      } else {
        /* In Face mode: tap triggers instant pet reaction with zero delay! */
        carmood_handle_top_single_click(&face_state, now_ms);
      }
    }

    if (app_state == APP_STATE_NORMAL && !in_muyu_mode && !in_flappy_mode && !in_brick_mode &&
        !in_shooter_mode && !in_pomodoro_mode &&
        carmood_face_should_restore_idle(&face_state, now_ms)) {
      carmood_face_restore_idle(&face_state, now_ms);
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
    if (s_has_lis3dh &&
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
        char new_turn = 'C', new_pitch = 'N';

        lis3dh_get_delta(ax, ay, az, &dx, &dy, &dz);
        lis3dh_eval_direction(dx, dy, dz, &new_turn, &new_pitch, &lr_val, &fb_val);

        int32_t ddx = dx - prev_dx;
        int32_t ddy = dy - prev_dy;
        int32_t ddz = dz - prev_dz;
        int32_t shake_lr = lr_val - prev_lr_val;
        int32_t shake_fb = fb_val - prev_fb_val;
        int32_t shake_val =
            (ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy) + (ddz < 0 ? -ddz : ddz);
        prev_dx = dx;
        prev_dy = dy;
        prev_dz = dz;
        prev_lr_val = lr_val;
        prev_fb_val = fb_val;
        carmood_ui_set_motion_input(lr_val, fb_val, shake_lr, shake_fb, shake_val);

        bool dir_changed = (new_turn != turn_char) || (new_pitch != pitch_char);
        if (dir_changed) {
          turn_char = new_turn;
          pitch_char = new_pitch;
          carmood_ui_set_direction_overlay(turn_char, pitch_char);

          ESP_LOGI(TAG, "方向变化: %c/%c (lr=%ld fb=%ld)", turn_char, pitch_char, (long)lr_val,
                   (long)fb_val);
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
          ESP_LOGI(TAG, "方向稳定: %c/%c (lr=%ld fb=%ld)", turn_char, pitch_char, (long)lr_val,
                   (long)fb_val);
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
          last_dir_log_ms = now_ms;
#endif
        }

#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
        ESP_LOGI(TAG, "LIS3DH raw=(%d,%d,%d) delta=(%ld,%ld,%ld)", ax, ay, az, (long)dx, (long)dy,
                 (long)dz);
#endif
      } else if (ret == ESP_OK) {
        prev_dx = 0;
        prev_dy = 0;
        prev_dz = 0;
        prev_lr_val = 0;
        prev_fb_val = 0;
        carmood_ui_set_motion_input(0, 0, 0, 0, 0);
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
        ESP_LOGI(TAG, "LIS3DH xyz(raw): x=%d y=%d z=%d", ax, ay, az);
#endif
      } else {
        prev_dx = 0;
        prev_dy = 0;
        prev_dz = 0;
        prev_lr_val = 0;
        prev_fb_val = 0;
        carmood_ui_set_motion_input(0, 0, 0, 0, 0);
        ESP_LOGW(TAG, "读取 LIS3DH xyz 失败: %s", esp_err_to_name(ret));
      }
#if CONFIG_CARMOOD_DEBUG_PERIODIC_LOG
      last_lis3dh_log_ms = now_ms;
#endif
    }

    if (app_state == APP_STATE_NORMAL && !in_muyu_mode && !in_flappy_mode) {
      carmood_face_update(&face_state, now_ms);
    }

    vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_TOUCH_SAMPLE_MS));
  }
}
