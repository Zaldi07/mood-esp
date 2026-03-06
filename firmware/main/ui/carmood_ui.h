#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 表情枚举，与 anim_*.h 数组一一对应 */
typedef enum {
    EXPR_IDLE        = 0,
    EXPR_BLINK       = 1,
    EXPR_HAPPY       = 2,
    EXPR_SLEEPY      = 3,
    EXPR_TURN_LEFT   = 4,
    EXPR_TURN_RIGHT  = 5,
    EXPR_SURPRISED   = 6,
    EXPR_ANGRY       = 7,
    EXPR_COUNT       = 8,
} carmood_expr_t;

/* 启动后台动画任务（在 oled_init() 之后调用一次） */
void carmood_ui_init(void);

/* 切换当前播放的表情（线程安全，可从任意任务调用） */
void carmood_ui_set_expression(carmood_expr_t expr);

/* 更新左上角方向叠加层（线程安全） */
void carmood_ui_set_direction_overlay(char turn_char, char pitch_char);

/* 显示校准引导文字（同时暂停动画任务，避免竞争屏幕） */
void carmood_ui_show_calibration(const char *line1, const char *line2);

/* 暂停动画任务但不改当前模式 */
void carmood_ui_pause_animation(void);

/* 校准结束后恢复动画播放（与 show_calibration 配对使用） */
void carmood_ui_resume_animation(void);

/* 进入木鱼模式（显示静止画面，等待敲击） */
void carmood_ui_enter_muyu(void);

/* 退出木鱼模式，恢复表情动画 */
void carmood_ui_exit_muyu(void);

/* 敲一次木鱼（播放一次敲击动画，播完自动回到静止画面） */
void carmood_ui_tap_muyu(void);

/* 查询当前是否处于木鱼模式 */
bool carmood_ui_is_muyu_mode(void);

/* 读取当前 UI 状态快照 */
void carmood_ui_get_status(carmood_expr_t *expr,
                           char *turn_char,
                           char *pitch_char,
                           bool *muyu_mode,
                           int *muyu_count);
