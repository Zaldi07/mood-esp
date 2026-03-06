#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * 渲染木鱼静止画面（清屏 + 画木鱼 + 画计数器）。
 * @param count 当前功德总数
 */
void muyu_render_idle(int count);

/**
 * 渲染木鱼敲击动画的一帧。
 * @param t_ms  敲击开始后的毫秒数
 * @param count 当前功德总数
 * @return true=动画还在播放, false=动画结束
 */
bool muyu_render_tap_frame(uint32_t t_ms, int count);
