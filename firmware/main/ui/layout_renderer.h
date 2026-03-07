#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LAYOUT_ITEM_TEXT = 0,
    LAYOUT_ITEM_LINE = 1,
    LAYOUT_ITEM_RECT = 2,
} layout_item_type_t;

typedef struct {
    layout_item_type_t type;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t x2;
    int16_t y2;
    uint8_t scale;
    bool filled;
    const char *text;
} layout_item_t;

typedef struct {
    const char *name;
    const layout_item_t *items;
    uint16_t item_count;
} layout_page_t;

void layout_render_page(const layout_page_t *page);
const layout_page_t *layout_get_default_page(void);
