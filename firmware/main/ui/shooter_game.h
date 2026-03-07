#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 游戏输入事件 */
typedef enum {
    SHOOTER_INPUT_NONE = 0,
    SHOOTER_INPUT_LEFT,
    SHOOTER_INPUT_RIGHT,
    SHOOTER_INPUT_FIRE,
    SHOOTER_INPUT_LEFT_HOLD,
    SHOOTER_INPUT_RIGHT_HOLD,
    SHOOTER_INPUT_RELEASE,
} shooter_input_t;

/**
 * 初始化打飞机游戏状态（进入游戏时调用）
 */
void shooter_game_init(void);

/**
 * 推送一个输入事件
 */
void shooter_game_push_input(shooter_input_t input);

/**
 * 渲染一帧并推进游戏逻辑
 * @return true=游戏进行中, false=游戏结束
 */
bool shooter_game_tick(void);

/**
 * 获取当前得分
 */
int shooter_game_get_score(void);

/**
 * 游戏是否已结束
 */
bool shooter_game_is_over(void);
