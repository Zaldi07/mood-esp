#include "drivers/touch_input.h"

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "touch_input";

#ifndef CONFIG_CARMOOD_TOUCH_GPIO
#define CONFIG_CARMOOD_TOUCH_GPIO 7
#endif

#ifndef CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT
#define CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT 1
#endif
#ifndef CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS
#define CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS 15
#endif
#ifndef CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS
#define CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS 400
#endif
#ifndef CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS
#define CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS 380
#endif

#define PIN_TOUCH ((gpio_num_t)CONFIG_CARMOOD_TOUCH_GPIO)

typedef struct {
  bool stable_pressed;
  int press_cnt;
  int release_cnt;
  int64_t press_start_ms;
  int64_t last_click_ms;
  uint8_t last_click_count;
} touch_key_state_t;

static touch_key_state_t s_key_state = {0};

esp_err_t touch_input_init(void) {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << PIN_TOUCH),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t err = gpio_config(&io_conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Touch GPIO %d config failed: %s", (int)PIN_TOUCH, esp_err_to_name(err));
    return err;
  }

  s_key_state.last_click_ms = -100000;
  ESP_LOGI(TAG, "Touch input initialized on GPIO %d (TTP223 digital mode)", (int)PIN_TOUCH);
  return ESP_OK;
}

esp_err_t touch_input_poll(touch_input_event_t *event) {
  if (event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  *event = (touch_input_event_t){0};
  int64_t now_ms = esp_timer_get_time() / 1000;

  int level = gpio_get_level(PIN_TOUCH);
  bool pressed = (level == 1); // TTP223 outputs HIGH on touch

  touch_key_state_t *state = &s_key_state;
  touch_input_key_event_t *key_event = &event->keys[TOUCH_KEY_TOP];

  if (!state->stable_pressed) {
    state->release_cnt = 0;
    state->press_cnt = pressed ? (state->press_cnt + 1) : 0;
    if (state->press_cnt >= CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT) {
      state->stable_pressed = true;
      state->press_start_ms = now_ms;
      state->press_cnt = 0;
      ESP_LOGD(TAG, "Touch PRESSED");
    }
  } else {
    key_event->press_ms = now_ms - state->press_start_ms;
    state->press_cnt = 0;
    state->release_cnt = (!pressed) ? (state->release_cnt + 1) : 0;
    if (state->release_cnt >= CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT) {
      state->stable_pressed = false;
      state->release_cnt = 0;
      key_event->released = true;

      int64_t duration = now_ms - state->press_start_ms;
      if (duration >= CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS &&
          duration <= CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS) {
        int64_t gap = now_ms - state->last_click_ms;
        key_event->gap_ms = gap;
        if (gap <= CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS && state->last_click_count == 1) {
          key_event->valid_click = true;
          key_event->double_click = true;
          key_event->click_count = 2;
          state->last_click_count = 2;
          ESP_LOGI(TAG, "Touch DOUBLE CLICK");
        } else {
          key_event->valid_click = true;
          key_event->double_click = false;
          key_event->click_count = 1;
          state->last_click_count = 1;
          ESP_LOGI(TAG, "Touch SINGLE CLICK");
        }
        state->last_click_ms = now_ms;
      } else {
        state->last_click_count = 0;
        state->last_click_ms = 0;
      }
    }
  }

  event->samples[TOUCH_KEY_TOP].stable_pressed = state->stable_pressed;
  event->samples[TOUCH_KEY_TOP].smooth = pressed ? 1000 : 0;
  event->samples[TOUCH_KEY_TOP].delta = pressed ? 1000 : 0;
  event->swipe = TOUCH_SWIPE_NONE;

  return ESP_OK;
}
