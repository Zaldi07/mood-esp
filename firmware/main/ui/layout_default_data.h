#pragma once

#include "ui/layout_renderer.h"

static const layout_item_t s_default_layout_items[] = {
    {
        .type = LAYOUT_ITEM_TEXT,
        .x = 8,
        .y = 8,
        .scale = 1,
        .text = "CARMOOD",
    },
    {
        .type = LAYOUT_ITEM_LINE,
        .x = 8,
        .y = 18,
        .x2 = 119,
        .y2 = 18,
    },
    {
        .type = LAYOUT_ITEM_TEXT,
        .x = 12,
        .y = 28,
        .scale = 3,
        .text = "12:45",
    },
    {
        .type = LAYOUT_ITEM_TEXT,
        .x = 12,
        .y = 52,
        .scale = 1,
        .text = "LAYOUT PAGE",
    },
    {
        .type = LAYOUT_ITEM_RECT,
        .x = 94,
        .y = 28,
        .w = 24,
        .h = 18,
        .filled = false,
    },
};

static const layout_page_t s_default_layout_page = {
    .name = "default",
    .items = s_default_layout_items,
    .item_count = sizeof(s_default_layout_items) / sizeof(s_default_layout_items[0]),
};
