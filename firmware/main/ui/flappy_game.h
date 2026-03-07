#pragma once

#include <stdbool.h>

void flappy_game_init(void);
void flappy_game_jump(void);
bool flappy_game_tick(void);
bool flappy_game_is_over(void);
int flappy_game_get_score(void);
