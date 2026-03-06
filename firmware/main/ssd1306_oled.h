#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

/**
 * 初始化 SPI 总线、Panel IO 和 SSD1306 面板
 * 完成后屏幕处于开启状态，内容为空
 */
esp_err_t oled_init(void);

/**
 * 清屏（全黑）
 */
esp_err_t oled_clear(void);

/**
 * 只清空帧缓冲（不 flush 到屏幕），用于实时绘制前的准备
 */
void oled_clear_buf(void);

/**
 * 全屏填充（全白）
 */
esp_err_t oled_fill(void);

/**
 * 将内部帧缓冲刷新到屏幕
 */
esp_err_t oled_flush(void);

/**
 * 在帧缓冲中设置单个像素
 * @param x 列坐标 [0, OLED_WIDTH)
 * @param y 行坐标 [0, OLED_HEIGHT)
 * @param on true=亮 false=灭
 */
void oled_set_pixel(int x, int y, bool on);

/**
 * 将外部位图数据绘制到帧缓冲
 * 数据格式：驱动内部页模式缓存（1024 字节）
 * @param data 位图数据，长度 = OLED_WIDTH * OLED_HEIGHT / 8
 */
void oled_draw_bitmap(const uint8_t *data);

#ifdef __cplusplus
}
#endif
