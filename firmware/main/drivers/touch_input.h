#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
  TOUCH_KEY_TOP = 0,
  TOUCH_KEY_UP,
  TOUCH_KEY_DOWN,
  TOUCH_KEY_COUNT,
} touch_key_t;

typedef struct {
  uint32_t benchmark;
  uint32_t smooth;
  int32_t delta;
  bool stable_pressed;
} touch_input_sample_t;

typedef struct {
  bool released;
  bool valid_click;
  bool double_click;
  uint8_t click_count;
  int64_t press_ms;
  int64_t gap_ms;
} touch_input_key_event_t;

typedef enum {
  TOUCH_SWIPE_NONE = 0,
  TOUCH_SWIPE_UP,
  TOUCH_SWIPE_DOWN,
} touch_swipe_t;

typedef struct {
  touch_input_key_event_t keys[TOUCH_KEY_COUNT];
  touch_input_sample_t samples[TOUCH_KEY_COUNT];
  touch_swipe_t swipe;
} touch_input_event_t;

esp_err_t touch_input_init(void);
esp_err_t touch_input_poll(touch_input_event_t *event);
