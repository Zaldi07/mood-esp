#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef void (*lis3dh_calib_prompt_fn_t)(const char *line1, const char *line2);

esp_err_t lis3dh_init(void);
esp_err_t lis3dh_rezero(void);
esp_err_t lis3dh_read_raw(int16_t *x, int16_t *y, int16_t *z);
esp_err_t lis3dh_run_guided_calibration(lis3dh_calib_prompt_fn_t prompt_fn);

bool lis3dh_is_zero_ready(void);
void lis3dh_get_delta(int16_t ax, int16_t ay, int16_t az, int32_t *dx, int32_t *dy,
                      int32_t *dz);
void lis3dh_eval_direction(int32_t dx, int32_t dy, int32_t dz, char *turn_state_char,
                           char *pitch_state_char, int32_t *lr_val,
                           int32_t *fb_val);
