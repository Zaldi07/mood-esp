#pragma once

#include <stdbool.h>
#include <stdint.h>

void brick_game_init(void);
void brick_game_set_tilt(int32_t lr_val);
void brick_game_step_left(void);
void brick_game_step_right(void);
bool brick_game_tick(void);
bool brick_game_is_finished(void);
