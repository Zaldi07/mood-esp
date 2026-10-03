#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the animated boot intro sequence (intro.gif frames) on SSD1306 OLED.
 *        Plays along with the space chiptune buzzer melody.
 *        User can tap top touch sensor (GPIO 7) to fast-forward/skip.
 *        Blocks until animation completes or is skipped.
 */
void boot_intro_run(void);

#ifdef __cplusplus
}
#endif
