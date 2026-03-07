#include "ui/layout_renderer.h"

#include <stddef.h>
#include <string.h>

#include "ssd1306_oled.h"
#include "ui/layout_default_data.h"

static bool layout_get_glyph_5x7(char c, uint8_t glyph[5])
{
    switch (c) {
        case 'A': { uint8_t g[5]={0x7E,0x09,0x09,0x09,0x7E}; memcpy(glyph,g,5); return true; }
        case 'C': { uint8_t g[5]={0x3E,0x41,0x41,0x41,0x22}; memcpy(glyph,g,5); return true; }
        case 'D': { uint8_t g[5]={0x7F,0x41,0x41,0x22,0x1C}; memcpy(glyph,g,5); return true; }
        case 'E': { uint8_t g[5]={0x7F,0x49,0x49,0x49,0x41}; memcpy(glyph,g,5); return true; }
        case 'G': { uint8_t g[5]={0x3E,0x41,0x49,0x49,0x7A}; memcpy(glyph,g,5); return true; }
        case 'L': { uint8_t g[5]={0x7F,0x40,0x40,0x40,0x40}; memcpy(glyph,g,5); return true; }
        case 'M': { uint8_t g[5]={0x7F,0x02,0x0C,0x02,0x7F}; memcpy(glyph,g,5); return true; }
        case 'O': { uint8_t g[5]={0x3E,0x41,0x41,0x41,0x3E}; memcpy(glyph,g,5); return true; }
        case 'P': { uint8_t g[5]={0x7F,0x09,0x09,0x09,0x06}; memcpy(glyph,g,5); return true; }
        case 'R': { uint8_t g[5]={0x7F,0x09,0x19,0x29,0x46}; memcpy(glyph,g,5); return true; }
        case 'T': { uint8_t g[5]={0x01,0x01,0x7F,0x01,0x01}; memcpy(glyph,g,5); return true; }
        case 'U': { uint8_t g[5]={0x3F,0x40,0x40,0x40,0x3F}; memcpy(glyph,g,5); return true; }
        case 'Y': { uint8_t g[5]={0x07,0x08,0x70,0x08,0x07}; memcpy(glyph,g,5); return true; }
        case '0': { uint8_t g[5]={0x3E,0x51,0x49,0x45,0x3E}; memcpy(glyph,g,5); return true; }
        case '1': { uint8_t g[5]={0x00,0x42,0x7F,0x40,0x00}; memcpy(glyph,g,5); return true; }
        case '2': { uint8_t g[5]={0x42,0x61,0x51,0x49,0x46}; memcpy(glyph,g,5); return true; }
        case '4': { uint8_t g[5]={0x18,0x14,0x12,0x7F,0x10}; memcpy(glyph,g,5); return true; }
        case '5': { uint8_t g[5]={0x27,0x45,0x45,0x45,0x39}; memcpy(glyph,g,5); return true; }
        case ':': { uint8_t g[5]={0x00,0x36,0x36,0x00,0x00}; memcpy(glyph,g,5); return true; }
        case ' ': { uint8_t g[5]={0x00,0x00,0x00,0x00,0x00}; memcpy(glyph,g,5); return true; }
        default:
            return false;
    }
}

static void layout_draw_char(int x, int y, char c, int scale)
{
    uint8_t glyph[5] = {0};
    if (!layout_get_glyph_5x7(c, glyph)) {
        return;
    }

    for (int col = 0; col < 5; ++col) {
        for (int row = 0; row < 7; ++row) {
            if (((glyph[col] >> row) & 0x01U) == 0) {
                continue;
            }
            for (int sx = 0; sx < scale; ++sx) {
                for (int sy = 0; sy < scale; ++sy) {
                    oled_set_pixel(x + col * scale + sx, y + row * scale + sy, true);
                }
            }
        }
    }
}

static void layout_draw_text(int x, int y, const char *text, int scale)
{
    if (text == NULL) {
        return;
    }
    int advance = 6 * scale;
    for (int i = 0; text[i] != '\0'; ++i) {
        layout_draw_char(x + i * advance, y, text[i], scale);
    }
}

static void layout_draw_line(int x1, int y1, int x2, int y2)
{
    int dx = x2 > x1 ? (x2 - x1) : (x1 - x2);
    int sx = x1 < x2 ? 1 : -1;
    int dy = y1 > y2 ? -(y1 - y2) : -(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        oled_set_pixel(x1, y1, true);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void layout_draw_rect(const layout_item_t *item)
{
    int x1 = item->x;
    int y1 = item->y;
    int x2 = item->x + item->w - 1;
    int y2 = item->y + item->h - 1;

    if (item->filled) {
        for (int y = y1; y <= y2; ++y) {
            for (int x = x1; x <= x2; ++x) {
                oled_set_pixel(x, y, true);
            }
        }
        return;
    }

    layout_draw_line(x1, y1, x2, y1);
    layout_draw_line(x1, y2, x2, y2);
    layout_draw_line(x1, y1, x1, y2);
    layout_draw_line(x2, y1, x2, y2);
}

void layout_render_page(const layout_page_t *page)
{
    if (page == NULL) {
        return;
    }

    oled_clear_buf();
    for (uint16_t i = 0; i < page->item_count; ++i) {
        const layout_item_t *item = &page->items[i];
        switch (item->type) {
            case LAYOUT_ITEM_TEXT:
                layout_draw_text(item->x, item->y, item->text, item->scale == 0 ? 1 : item->scale);
                break;
            case LAYOUT_ITEM_LINE:
                layout_draw_line(item->x, item->y, item->x2, item->y2);
                break;
            case LAYOUT_ITEM_RECT:
                layout_draw_rect(item);
                break;
            default:
                break;
        }
    }
    oled_flush();
}

const layout_page_t *layout_get_default_page(void)
{
    return &s_default_layout_page;
}
