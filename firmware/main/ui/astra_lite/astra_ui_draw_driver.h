/* astra-lite draw driver — ESP32 + u8g2 + SSD1306 适配层 */
#ifndef ASTRA_UI_DRAW_DRIVER_H
#define ASTRA_UI_DRAW_DRIVER_H

#include "libs/u8g2/u8g2.h"
#include "ssd1306_oled.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_HEIGHT 64
#define OLED_WIDTH  128

extern u8g2_t g_astra_u8g2;
extern const uint8_t u8g2_font_my_chinese[];

/* 中文字体（12px WQY，自定义导出） */

uint32_t astra_get_ticks(void);
void     astra_delay(uint32_t ms);

#define get_ticks()                 astra_get_ticks()
#define delay(ms)                   astra_delay(ms)
#define oled_set_font(font)         u8g2_SetFont(&g_astra_u8g2, (const uint8_t *)(font))
#define oled_draw_str(x, y, str)    u8g2_DrawStr(&g_astra_u8g2, (x), (y), (str))
#define oled_draw_UTF8(x, y, str)   u8g2_DrawUTF8(&g_astra_u8g2, (x), (y), (str))
#define oled_get_str_width(str)     u8g2_GetStrWidth(&g_astra_u8g2, (str))
#define oled_get_UTF8_width(str)    u8g2_GetUTF8Width(&g_astra_u8g2, (str))
#define oled_get_str_height()       u8g2_GetMaxCharHeight(&g_astra_u8g2)
#define oled_draw_pixel(x, y)       u8g2_DrawPixel(&g_astra_u8g2, (x), (y))
#define oled_draw_circle(x, y, r)   u8g2_DrawCircle(&g_astra_u8g2, (x), (y), (r), U8G2_DRAW_ALL)
#define oled_draw_R_box(x, y, w, h, r)   u8g2_DrawRBox(&g_astra_u8g2, (x), (y), (w), (h), (r))
#define oled_draw_box(x, y, w, h)        u8g2_DrawBox(&g_astra_u8g2, (x), (y), (w), (h))
#define oled_draw_frame(x, y, w, h)      u8g2_DrawFrame(&g_astra_u8g2, (x), (y), (w), (h))
#define oled_draw_R_frame(x, y, w, h, r) u8g2_DrawRFrame(&g_astra_u8g2, (x), (y), (w), (h), (r))
#define oled_draw_H_line(x, y, l)        u8g2_DrawHLine(&g_astra_u8g2, (x), (y), (l))
#define oled_draw_V_line(x, y, h)        u8g2_DrawVLine(&g_astra_u8g2, (x), (y), (h))
#define oled_draw_line(x1, y1, x2, y2)   u8g2_DrawLine(&g_astra_u8g2, (x1), (y1), (x2), (y2))
#define oled_draw_bMP(x, y, w, h, bm)    u8g2_DrawXBMP(&g_astra_u8g2, (x), (y), (w), (h), (bm))
#define oled_set_draw_color(c)            u8g2_SetDrawColor(&g_astra_u8g2, (c))
#define oled_set_font_mode(m)             u8g2_SetFontMode(&g_astra_u8g2, (m))
#define oled_set_font_direction(d)        u8g2_SetFontDirection(&g_astra_u8g2, (d))
#define oled_clear_buffer()               u8g2_ClearBuffer(&g_astra_u8g2)
#define oled_send_buffer()                do { oled_draw_bitmap(u8g2_GetBufferPtr(&g_astra_u8g2)); oled_flush(); } while(0)
#define oled_send_area_buffer(x, y, w, h) oled_send_buffer()

/* 虚线绘制（u8g2 不提供，手动实现） */
static inline void oled_draw_H_dotted_line(int16_t x, int16_t y, int16_t l)
{
    for (int16_t i = 0; i < l; i += 2)
        u8g2_DrawPixel(&g_astra_u8g2, x + i, y);
}

static inline void oled_draw_V_dotted_line(int16_t x, int16_t y, int16_t h)
{
    for (int16_t i = 0; i < h; i += 2)
        u8g2_DrawPixel(&g_astra_u8g2, x, y + i);
}

void astra_ui_driver_init(void);

#ifdef __cplusplus
}
#endif

#endif /* ASTRA_UI_DRAW_DRIVER_H */
