#include "ui/muyu_renderer.h"

#include <string.h>
#include "ui/astra_lite/astra_ui_draw_driver.h"

#define W OLED_WIDTH
#define H OLED_HEIGHT

#define TAP_DURATION_MS 800

/* ------------------------------------------------------------------ */
/* 48x48 木鱼像素位图（每行 48 像素 = 6 字节，共 48 行 = 288 字节）        */
/* 1=白色像素，MSB 在左。手绘经典木鱼造型：                                 */
/*   圆鼓身体 + 顶部把手 + 正面鱼嘴开口 + 底部支撑                          */
/* ------------------------------------------------------------------ */
static const uint8_t s_muyu_48x48[288] = {
    0x00, 0x1F, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x7F, 0xFE, 0x00, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0x80, 0x00, 0x00,
    0x01, 0xFF, 0xFF, 0xE0, 0x00, 0x00,
    0x01, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x01, 0xFF, 0xFF, 0xF0, 0x00, 0x00,
    0x01, 0xFF, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xF8, 0x00, 0x00,
    0x00, 0x7F, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xC0, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xE0, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xF0, 0x00,
    0x00, 0x0F, 0xFF, 0xFF, 0xF8, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFC, 0x00,
    0x00, 0x3F, 0xFF, 0xFF, 0xFE, 0x00,
    0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF8,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x98,
    0x0F, 0xFF, 0xFF, 0xFF, 0xFE, 0x38,
    0x0F, 0xFF, 0xFF, 0xFF, 0xF0, 0xF8,
    0x0D, 0xFF, 0xFF, 0xFF, 0x81, 0xF8,
    0x0E, 0x1F, 0xFF, 0xF8, 0x07, 0xF8,
    0x07, 0x80, 0xFF, 0x00, 0x1F, 0xF0,
    0x07, 0xE0, 0x00, 0x00, 0x7F, 0xF0,
    0x07, 0xF8, 0x00, 0x01, 0xFF, 0xF0,
    0x03, 0xFF, 0x00, 0x0F, 0xFF, 0xE0,
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0,
    0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x3F, 0xFF, 0xFF, 0xFE, 0x00,
    0x00, 0x1F, 0xFF, 0xFF, 0xFC, 0x00,
    0x00, 0x07, 0xFF, 0xFF, 0xF0, 0x00,
    0x00, 0x03, 0xFF, 0xFF, 0xE0, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00,
};

/* 48x48 位图绘制到指定位置，支持缩放（scale=1 正常，偏移 ox/oy） */
static void draw_muyu_bitmap(int ox, int oy)
{
    for (int row = 0; row < 48; row++) {
        for (int col = 0; col < 48; col++) {
            int byte_idx = row * 6 + (col / 8);
            int bit_idx  = 7 - (col % 8);
            if ((s_muyu_48x48[byte_idx] >> bit_idx) & 1) {
                int px = ox + col;
                int py = oy + row;
                if (px >= 0 && px < W && py >= 0 && py < H)
                    oled_draw_pixel(px, py);
            }
        }
    }
}

/* 缩放绘制：简易最近邻缩放，scale_pct = 缩放百分比(100=原大小) */
static void draw_muyu_scaled(int ox, int oy, int scale_pct)
{
    int dst_w = 48 * scale_pct / 100;
    int dst_h = 48 * scale_pct / 100;
    int off_x = ox + (48 - dst_w) / 2;
    int off_y = oy + (48 - dst_h) / 2;

    for (int dy = 0; dy < dst_h; dy++) {
        int src_row = dy * 48 / dst_h;
        for (int dx = 0; dx < dst_w; dx++) {
            int src_col = dx * 48 / dst_w;
            int byte_idx = src_row * 6 + (src_col / 8);
            int bit_idx  = 7 - (src_col % 8);
            if ((s_muyu_48x48[byte_idx] >> bit_idx) & 1) {
                int px = off_x + dx;
                int py = off_y + dy;
                if (px >= 0 && px < W && py >= 0 && py < H)
                    oled_draw_pixel(px, py);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* 5x7 字形：+ 和数字                                                   */
/* ------------------------------------------------------------------ */

static const uint8_t s_glyph_plus[5]  = {0x08, 0x08, 0x3E, 0x08, 0x08};
static const uint8_t s_glyph_num[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static void draw_glyph(int x, int y, const uint8_t glyph[5], int scale)
{
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++) {
            if (!((glyph[col] >> row) & 1)) continue;
            for (int sx = 0; sx < scale; sx++)
                for (int sy = 0; sy < scale; sy++) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        oled_draw_pixel(px, py);
                }
        }
}

static void draw_number(int x, int y, int num, int scale)
{
    char buf[8];
    int len = 0;
    if (num <= 0) {
        buf[0] = '0'; len = 1;
    } else {
        int tmp = num;
        while (tmp > 0 && len < 7) {
            buf[len++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        for (int i = 0; i < len / 2; i++) {
            char c = buf[i]; buf[i] = buf[len - 1 - i]; buf[len - 1 - i] = c;
        }
    }
    int cw = 6 * scale;
    for (int i = 0; i < len; i++) {
        int digit = buf[i] - '0';
        draw_glyph(x + i * cw, y, s_glyph_num[digit], scale);
    }
}

/* ------------------------------------------------------------------ */
/* 公开接口                                                             */
/* ------------------------------------------------------------------ */

/* 木鱼居中偏左（48x48 位图放在 x=14, y=8 的位置） */
#define MUYU_X 14
#define MUYU_Y 8

void muyu_render_idle(int count)
{
    oled_clear_buffer();
    draw_muyu_bitmap(MUYU_X, MUYU_Y);

    /* 右下角显示计数 */
    draw_glyph(92, 44, s_glyph_plus, 1);
    draw_number(99, 44, count, 1);

    oled_send_buffer();
}

bool muyu_render_tap_frame(uint32_t t_ms, int count)
{
    if (t_ms >= TAP_DURATION_MS) return false;

    oled_clear_buffer();

    /* 敲击效果：木鱼先缩小再弹回 */
    int scale_pct = 100;
    if (t_ms < 80) {
        scale_pct = 100 - (int)(15 * t_ms / 80);  /* 100→85 */
    } else if (t_ms < 200) {
        int dt = t_ms - 80;
        scale_pct = 85 + (int)(20 * dt / 120);     /* 85→105 过冲 */
    } else if (t_ms < 300) {
        int dt = t_ms - 200;
        scale_pct = 105 - (int)(5 * dt / 100);     /* 105→100 归位 */
    }

    draw_muyu_scaled(MUYU_X, MUYU_Y, scale_pct);

    /* "+1" 飘上去的动画 */
    if (t_ms < 600) {
        int text_x = 78;
        int text_y = 24 - (int)(22 * t_ms / 600);
        int scale = 2;
        draw_glyph(text_x, text_y, s_glyph_plus, scale);
        draw_glyph(text_x + 13, text_y, s_glyph_num[1], scale);
    }

    /* 右下角计数 */
    draw_glyph(92, 44, s_glyph_plus, 1);
    draw_number(99, 44, count, 1);

    oled_send_buffer();
    return true;
}
