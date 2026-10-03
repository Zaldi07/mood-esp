#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POMODORO_PHASE_FOCUS = 0,
    POMODORO_PHASE_BREAK = 1,
} pomodoro_phase_t;

typedef enum {
    POMODORO_STATE_PAUSED = 0,
    POMODORO_STATE_RUNNING = 1,
    POMODORO_STATE_DONE = 2,
} pomodoro_state_t;

/**
 * @brief Initialize Pomodoro Timer with default 25min focus
 */
void pomodoro_init(void);

/**
 * @brief Render frame and tick countdown timer
 */
void pomodoro_tick(void);

/**
 * @brief Handle single tap touch input (Start / Pause / Next session)
 */
void pomodoro_tap(void);

/**
 * @brief Handle double tap touch input (Reset timer / switch focus<->break when paused)
 */
void pomodoro_double_tap(void);

/**
 * @brief Get remaining seconds in current session
 */
uint32_t pomodoro_get_remaining_sec(void);

/**
 * @brief Check if timer is actively running
 */
bool pomodoro_is_running(void);

/**
 * @brief Get count of completed focus sessions
 */
uint16_t pomodoro_get_completed_cycles(void);

#ifdef __cplusplus
}
#endif
