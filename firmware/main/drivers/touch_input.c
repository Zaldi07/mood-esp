#include "drivers/touch_input.h"

#include <stdint.h>

#include "driver/touch_sens.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "touch_input";

#define TOUCH_SAMPLE_CFG_USED 1
#define TOUCH_INIT_SCAN_TIMES 3

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

static const int s_touch_channels[TOUCH_KEY_COUNT] = {
    [TOUCH_KEY_TOP] = 5,
    [TOUCH_KEY_UP] = 7,
    [TOUCH_KEY_DOWN] = 6,
};

typedef struct {
  bool stable_pressed;
  int press_cnt;
  int release_cnt;
  int64_t press_start_ms;
  int64_t last_click_ms;
  uint8_t last_click_count;
} touch_key_state_t;

static touch_sensor_handle_t s_touch = NULL;
static touch_channel_handle_t s_touch_chans[TOUCH_KEY_COUNT] = {};
static touch_key_state_t s_key_states[TOUCH_KEY_COUNT] = {};

static int64_t s_last_debug_log_ms = 0;
#define DEBUG_LOG_INTERVAL_MS 500

/* 滑动检测：记录 UP/DOWN 通道最近一次 press 时间 */
#define SWIPE_WINDOW_MS 400
static int64_t s_swipe_press_ms[TOUCH_KEY_COUNT] = {0};
static bool    s_swipe_consumed = false;

static void touch_reset_event(touch_input_event_t *event) {
  *event = (touch_input_event_t){0};
  for (int i = 0; i < TOUCH_KEY_COUNT; ++i) {
    event->samples[i].stable_pressed = s_key_states[i].stable_pressed;
  }
}

esp_err_t touch_input_init(void) {
  touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
      TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(
          500, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V2)};
  touch_sensor_config_t sens_cfg =
      TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(TOUCH_SAMPLE_CFG_USED, sample_cfg);
  ESP_RETURN_ON_ERROR(
      touch_sensor_new_controller(&sens_cfg, &s_touch), TAG,
      "触摸控制器创建失败");

  touch_channel_config_t chan_cfg = {
      .active_thresh = {CONFIG_CARMOOD_TOUCH_PRESS_THRESHOLD},
      .charge_speed = TOUCH_CHARGE_SPEED_7,
      .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT,
  };

  for (int i = 0; i < TOUCH_KEY_COUNT; i++) {
    ESP_RETURN_ON_ERROR(
        touch_sensor_new_channel(
            s_touch, s_touch_channels[i], &chan_cfg, &s_touch_chans[i]),
        TAG, "触摸通道 CH%d 创建失败", s_touch_channels[i]);

    touch_chan_info_t chan_info = {};
    ESP_RETURN_ON_ERROR(
        touch_sensor_get_channel_info(s_touch_chans[i], &chan_info), TAG,
        "读取触摸通道信息失败");
    ESP_LOGI(
        TAG, "触摸通道: key=%d CH=%d GPIO=%d", i, chan_info.chan_id,
        chan_info.chan_gpio);

    s_key_states[i].last_click_ms = -100000;
  }

  touch_sensor_filter_config_t filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
  ESP_RETURN_ON_ERROR(
      touch_sensor_config_filter(s_touch, &filter_cfg), TAG,
      "触摸滤波配置失败");

  ESP_RETURN_ON_ERROR(touch_sensor_enable(s_touch), TAG, "触摸使能失败");
  for (int i = 0; i < TOUCH_INIT_SCAN_TIMES; i++) {
    ESP_RETURN_ON_ERROR(
        touch_sensor_trigger_oneshot_scanning(s_touch, 2000), TAG,
        "触摸初始扫描失败");
  }

  ESP_RETURN_ON_ERROR(
      touch_sensor_start_continuous_scanning(s_touch), TAG,
      "触摸连续扫描启动失败");
  return ESP_OK;
}

static void touch_update_key(
    touch_key_t key, int64_t now_ms, int32_t delta, touch_input_event_t *event) {
  touch_key_state_t *state = &s_key_states[key];
  touch_input_key_event_t *key_event = &event->keys[key];

  bool over_press = delta >= CONFIG_CARMOOD_TOUCH_PRESS_THRESHOLD;
  bool under_release = delta <= CONFIG_CARMOOD_TOUCH_RELEASE_THRESHOLD;

  if (!state->stable_pressed) {
    state->release_cnt = 0;
    state->press_cnt = over_press ? (state->press_cnt + 1) : 0;
    if (state->press_cnt >= CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT) {
      state->stable_pressed = true;
      state->press_start_ms = now_ms;
      state->press_cnt = 0;
      if (key == TOUCH_KEY_UP || key == TOUCH_KEY_DOWN) {
        s_swipe_press_ms[key] = now_ms;
        s_swipe_consumed = false;
      }
      ESP_LOGI(
          TAG, "状态变化: PRESSED key=%d delta=%ld", key, (long)delta);
    }
  } else {
    key_event->press_ms = now_ms - state->press_start_ms;

    state->press_cnt = 0;
    state->release_cnt = under_release ? (state->release_cnt + 1) : 0;
    if (state->release_cnt >= CONFIG_CARMOOD_TOUCH_DEBOUNCE_COUNT) {
      int64_t press_ms = now_ms - state->press_start_ms;
      int64_t gap_ms = now_ms - state->last_click_ms;
      int64_t min_gap_ms = CONFIG_CARMOOD_TOUCH_MIN_GAP_MS;
      if (key == TOUCH_KEY_TOP && min_gap_ms > CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS) {
        min_gap_ms = CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS;
      }
      bool valid_click =
          (press_ms >= CONFIG_CARMOOD_TOUCH_CLICK_MIN_MS) &&
          (press_ms <= CONFIG_CARMOOD_TOUCH_CLICK_MAX_MS) &&
          (gap_ms >= min_gap_ms);
      bool supports_double_click = (key == TOUCH_KEY_TOP);
      bool is_double_click =
          valid_click &&
          supports_double_click &&
          state->last_click_count == 1 &&
          gap_ms <= CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS;

      state->stable_pressed = false;
      state->release_cnt = 0;
      if (valid_click) {
        state->last_click_ms = now_ms;
        state->last_click_count = is_double_click ? 2 : 1;
      } else if (supports_double_click &&
                 gap_ms > CONFIG_CARMOOD_TOP_DOUBLE_CLICK_MS) {
        state->last_click_count = 0;
      }

      key_event->released = true;
      key_event->valid_click = valid_click;
      key_event->double_click = is_double_click;
      key_event->click_count = is_double_click ? 2 : (valid_click ? 1 : 0);
      key_event->press_ms = press_ms;
      key_event->gap_ms = gap_ms;
      ESP_LOGI(
          TAG, "状态变化: RELEASED key=%d delta=%ld click=%d count=%u double=%d",
          key, (long)delta, valid_click, (unsigned)key_event->click_count, is_double_click);
    }
  }

  event->samples[key].stable_pressed = state->stable_pressed;
}

esp_err_t touch_input_poll(touch_input_event_t *event) {
  if (event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  touch_reset_event(event);

  int64_t now_ms = esp_timer_get_time() / 1000;
  int32_t deltas[TOUCH_KEY_COUNT] = {};

  for (int i = 0; i < TOUCH_KEY_COUNT; i++) {
    uint32_t benchmark[TOUCH_SAMPLE_CFG_NUM] = {};
    uint32_t smooth[TOUCH_SAMPLE_CFG_NUM] = {};
    ESP_RETURN_ON_ERROR(
        touch_channel_read_data(
            s_touch_chans[i], TOUCH_CHAN_DATA_TYPE_BENCHMARK, benchmark),
        TAG, "CH%d benchmark 读取失败", s_touch_channels[i]);
    ESP_RETURN_ON_ERROR(
        touch_channel_read_data(
            s_touch_chans[i], TOUCH_CHAN_DATA_TYPE_SMOOTH, smooth),
        TAG, "CH%d smooth 读取失败", s_touch_channels[i]);

    deltas[i] = (int32_t)smooth[0] - (int32_t)benchmark[0];
    event->samples[i].benchmark = benchmark[0];
    event->samples[i].smooth = smooth[0];
    event->samples[i].delta = deltas[i];
  }

  if (now_ms - s_last_debug_log_ms >= DEBUG_LOG_INTERVAL_MS) {
    s_last_debug_log_ms = now_ms;
    ESP_LOGI(
        TAG, "CH5=%ld CH7=%ld CH6=%ld",
        (long)deltas[TOUCH_KEY_TOP],
        (long)deltas[TOUCH_KEY_UP],
        (long)deltas[TOUCH_KEY_DOWN]);
  }

  for (int i = 0; i < TOUCH_KEY_COUNT; ++i) {
    touch_update_key((touch_key_t)i, now_ms, deltas[i], event);
  }

  /* 滑动检测：两个通道在窗口内先后被按下 */
  if (!s_swipe_consumed &&
      s_swipe_press_ms[TOUCH_KEY_UP] > 0 &&
      s_swipe_press_ms[TOUCH_KEY_DOWN] > 0) {
    int64_t diff = s_swipe_press_ms[TOUCH_KEY_DOWN] - s_swipe_press_ms[TOUCH_KEY_UP];
    if (diff > 0 && diff <= SWIPE_WINDOW_MS) {
      event->swipe = TOUCH_SWIPE_DOWN;
      s_swipe_consumed = true;
      ESP_LOGI(TAG, "滑动检测: 向下 (diff=%lldms)", (long long)diff);
    } else if (diff < 0 && -diff <= SWIPE_WINDOW_MS) {
      event->swipe = TOUCH_SWIPE_UP;
      s_swipe_consumed = true;
      ESP_LOGI(TAG, "滑动检测: 向上 (diff=%lldms)", (long long)-diff);
    }
  }

  /* 两个通道都释放后重置滑动状态 */
  if (!event->samples[TOUCH_KEY_UP].stable_pressed &&
      !event->samples[TOUCH_KEY_DOWN].stable_pressed) {
    s_swipe_press_ms[TOUCH_KEY_UP] = 0;
    s_swipe_press_ms[TOUCH_KEY_DOWN] = 0;
    s_swipe_consumed = false;
  }

  return ESP_OK;
}
