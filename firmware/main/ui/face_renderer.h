#pragma once

#include <stdint.h>
#include "ssd1306_oled.h"
#include "ui/carmood_ui.h"

/**
 * 根据当前时间 t_ms 选取预渲染帧，RLE 解压后写入 framebuffer（不 flush）。
 * 帧数据由 Python 脚本自动生成。
 */
void face_render_frame(carmood_expr_t expr, uint32_t t_ms);

/**
 * 在旧表情和新表情之间做一次短暂的单色抖动过渡，避免状态硬切。
 * progress_255: 0=全旧帧，255=全新帧。
 */
void face_render_transition(carmood_expr_t from_expr,
                            uint32_t from_t_ms,
                            carmood_expr_t to_expr,
                            uint32_t to_t_ms,
                            uint8_t progress_255);
