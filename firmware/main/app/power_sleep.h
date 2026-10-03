#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_SLEEP_HOLD_THRESHOLD_MS 2000 /* Start showing countdown after 2s */
#define POWER_SLEEP_HOLD_TRIGGER_MS   5000 /* Enter sleep mode after 5s */

/**
 * @brief Draw visual sleep countdown and progress bar on OLED while holding touch
 * @param hold_ms Current touch hold duration in milliseconds
 */
void power_sleep_draw_countdown(int64_t hold_ms);

/**
 * @brief Enter ultra-low-power sleep mode with GPIO touch wakeup.
 *        Plays sleep chime, turns off OLED display (0 mA),
 *        waits for touch release, and enters Light Sleep.
 *        When touched again on GPIO 7, wakes up and restarts cleanly.
 */
void power_sleep_enter(void);

#ifdef __cplusplus
}
#endif
