#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUZZER_DEFAULT_PIN 5

/**
 * @brief Initialize buzzer driver on GPIO 5 using LEDC PWM
 */
esp_err_t buzzer_init(void);

/**
 * @brief Turn on continuous tone at specified frequency
 * @param freq_hz Frequency in Hz (e.g. 2500)
 */
void buzzer_tone_on(uint32_t freq_hz);

/**
 * @brief Turn off buzzer sound immediately
 */
void buzzer_off(void);

/**
 * @brief Play a single short beep (non-blocking)
 * @param duration_ms Duration in milliseconds (e.g. 50ms)
 * @param freq_hz Frequency in Hz (e.g. 2700)
 */
void buzzer_beep(uint32_t duration_ms, uint32_t freq_hz);

/**
 * @brief Start Pomodoro alarm pattern (repeating rhythmic triple-beep)
 */
void buzzer_start_alarm(void);

/**
 * @brief Stop any ongoing alarm or beep
 */
void buzzer_stop(void);

/**
 * @brief Check if alarm is currently active
 */
bool buzzer_is_alarm_active(void);

/**
 * @brief Play space chiptune intro melody on buzzer (blocking)
 */
void buzzer_play_intro_melody(void);

/**
 * @brief Play space chiptune intro melody asynchronously in a background FreeRTOS task
 */
void buzzer_play_intro_melody_async(void);

/**
 * @brief Play soft falling sleep melody on buzzer (blocking)
 */
void buzzer_play_sleep_melody(void);

/**
 * @brief Periodic tick function to update alarm/beep pattern
 * Call this in main loop or UI tick task (~30ms intervals)
 */
void buzzer_tick(void);

#ifdef __cplusplus
}
#endif
