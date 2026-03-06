#pragma once

#include <stdint.h>
#include "ssd1306_oled.h"
#include "ui/carmood_ui.h"

/**
 * 根据当前时间 t_ms 选取预渲染帧，RLE 解压后写入 framebuffer（不 flush）。
 * 帧数据由 Python 脚本自动生成。
 */
void face_render_frame(carmood_expr_t expr, uint32_t t_ms);
