#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_QUOTE_COUNT 26

/**
 * @brief Get total number of quotes
 */
uint32_t boot_quote_get_count(void);

/**
 * @brief Get quote string by index (0 to BOOT_QUOTE_COUNT - 1)
 */
const char* boot_quote_get_text(uint32_t index);

/**
 * @brief Pick next quote index (randomized, avoiding repeats across boots)
 */
uint32_t boot_quote_get_next_index(void);

/**
 * @brief Run the animated boot quote intro sequence on SSD1306 OLED.
 *        Plays typewriter text animation with smooth auto-scroll.
 *        User can tap top touch button to fast-forward or skip.
 *        Blocks until intro completes or is skipped.
 */
void boot_quote_run_intro(void);

#ifdef __cplusplus
}
#endif
