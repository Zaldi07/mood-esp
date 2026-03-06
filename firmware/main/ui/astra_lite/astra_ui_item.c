/* astra-lite item 实现 */
#include "ui/astra_lite/astra_ui_item.h"
#include "ui/astra_lite/astra_ui_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const void *astra_font_current;

void astra_set_font(const void *_font)
{
    if (_font != astra_font_current) {
        oled_set_font(_font);
        astra_font_current = _font;
    }
}

/* ---- 信息栏 ---- */
astra_info_bar_t astra_info_bar = {
    .content = NULL, .span = 1,
    .y_info_bar = -2.0f * INFO_BAR_HEIGHT, .y_info_bar_trg = -2.0f * INFO_BAR_HEIGHT,
    .w_info_bar = 80, .w_info_bar_trg = 80,
    .is_running = false, .time_start = 0, .time = 1
};

void astra_push_info_bar(char *_content, uint16_t _span)
{
    astra_info_bar.time    = get_ticks();
    astra_info_bar.content = _content;
    astra_info_bar.span    = _span;
    astra_info_bar.is_running = false;

    if (!astra_info_bar.is_running) {
        astra_info_bar.time_start    = get_ticks();
        astra_info_bar.y_info_bar_trg = 0;
        astra_info_bar.is_running    = true;
    }

    astra_set_font(u8g2_font_my_chinese);
    astra_info_bar.w_info_bar_trg =
        (float)oled_get_UTF8_width(astra_info_bar.content) + INFO_BAR_OFFSET;
}

/* ---- 弹窗 ---- */
astra_pop_up_t astra_pop_up = {
    .content = NULL, .span = 1,
    .y_pop_up = -2.0f * POP_UP_HEIGHT, .y_pop_up_trg = -2.0f * POP_UP_HEIGHT,
    .w_pop_up = 80, .w_pop_up_trg = 80,
    .is_running = false, .time_start = 0, .time = 1
};

void astra_push_pop_up(char *_content, uint16_t _span)
{
    astra_pop_up.time    = get_ticks();
    astra_pop_up.content = _content;
    astra_pop_up.span    = _span;
    astra_pop_up.is_running = false;

    if (!astra_pop_up.is_running) {
        astra_pop_up.time_start    = get_ticks();
        astra_pop_up.y_pop_up_trg = 20;
        astra_pop_up.is_running   = true;
    }

    astra_set_font(u8g2_font_my_chinese);
    astra_pop_up.w_pop_up_trg =
        (float)oled_get_UTF8_width(astra_pop_up.content) + POP_UP_OFFSET;
}

/* ---- 列表项构造 ---- */
astra_list_item_t *astra_get_root_list(void)
{
    static astra_list_item_t *root = NULL;
    if (root == NULL) {
        root = calloc(1, sizeof(astra_list_item_t));
        root->type    = list_item;
        root->content = "root";
    }
    return root;
}

astra_switch_item_t *astra_to_switch_item(astra_list_item_t *item)
{
    if (item && item->type == switch_item)
        return (astra_switch_item_t *)item;
    return (astra_switch_item_t *)astra_get_root_list();
}

astra_button_item_t *astra_to_button_item(astra_list_item_t *item)
{
    if (item && item->type == button_item)
        return (astra_button_item_t *)item;
    return (astra_button_item_t *)astra_get_root_list();
}

astra_slider_item_t *astra_to_slider_item(astra_list_item_t *item)
{
    if (item && item->type == slider_item)
        return (astra_slider_item_t *)item;
    return (astra_slider_item_t *)astra_get_root_list();
}

astra_user_item_t *astra_to_user_item(astra_list_item_t *item)
{
    if (item && item->type == user_item)
        return (astra_user_item_t *)item;
    return (astra_user_item_t *)astra_get_root_list();
}

astra_list_item_t *astra_new_list_item(char *content, astra_list_item_icon_t icon)
{
    astra_list_item_t *p = calloc(1, sizeof(astra_list_item_t));
    p->type    = list_item;
    p->content = content;
    p->icon    = (icon == default_icon) ? list_icon : icon;
    return p;
}

astra_list_item_t *astra_new_switch_item(char *content, bool *value,
                                         void (*init_fn)(void), void (*exit_fn)(void),
                                         astra_list_item_icon_t icon)
{
    astra_switch_item_t *p = calloc(1, sizeof(astra_switch_item_t));
    p->base_item.type    = switch_item;
    p->base_item.content = content;
    p->base_item.icon    = (icon == default_icon) ? switch_icon : icon;
    p->value         = value;
    p->init_function = init_fn;
    p->exit_function = exit_fn;
    return (astra_list_item_t *)p;
}

astra_list_item_t *astra_new_button_item(char *content, void (*exit_fn)(void),
                                         astra_list_item_icon_t icon)
{
    astra_button_item_t *p = calloc(1, sizeof(astra_button_item_t));
    p->base_item.type    = button_item;
    p->base_item.content = content;
    p->base_item.icon    = (icon == default_icon) ? plus_icon : icon;
    p->exit_function     = exit_fn;
    return (astra_list_item_t *)p;
}

astra_list_item_t *astra_new_slider_item(char *content, int16_t *value,
                                         uint8_t step, int16_t min, int16_t max,
                                         void (*init_fn)(void), void (*exit_fn)(void),
                                         astra_list_item_icon_t icon)
{
    astra_slider_item_t *p = calloc(1, sizeof(astra_slider_item_t));
    p->base_item.type    = slider_item;
    p->base_item.content = content;
    p->base_item.icon    = (icon == default_icon) ? slider_icon : icon;
    p->value         = value;
    p->value_step    = step;
    p->value_min     = min;
    p->value_max     = max;
    p->init_function = init_fn;
    p->exit_function = exit_fn;
    return (astra_list_item_t *)p;
}

astra_list_item_t *astra_new_user_item(char *content,
                                       void (*init_fn)(void), void (*loop_fn)(void),
                                       void (*exit_fn)(void),
                                       astra_list_item_icon_t icon)
{
    astra_user_item_t *p = calloc(1, sizeof(astra_user_item_t));
    p->base_item.type    = user_item;
    p->base_item.content = content;
    p->base_item.icon    = (icon == default_icon) ? user_icon : icon;
    p->init_function     = init_fn;
    p->loop_function     = loop_fn;
    p->exit_function     = exit_fn;
    return (astra_list_item_t *)p;
}

/* ---- 选择器 ---- */
astra_selector_t astra_selector;

astra_selector_t *astra_get_selector(void) { return &astra_selector; }

bool astra_bind_item_to_selector(astra_list_item_t *item)
{
    if (!item) return false;

    uint8_t idx = 0;
    if (item->parent) {
        for (uint8_t i = 0; i < item->parent->child_num; i++) {
            if (item->parent->child_list_item[i] == item) {
                idx = i; break;
            }
        }
    }

    if (astra_selector.selected_item == NULL) {
        astra_selector.y_selector = 2.0f * SCREEN_HEIGHT;
        astra_selector.h_selector = 160;
    }
    astra_selector.selected_index = idx;
    astra_selector.selected_item  = item;
    return true;
}

bool astra_refresh_list_value = true;

void astra_selector_go_next_item(void)
{
    if (astra_selector.selected_item->type == slider_item &&
        astra_to_slider_item(astra_selector.selected_item)->is_confirmed) {
        astra_slider_item_t *si = astra_to_slider_item(astra_selector.selected_item);
        *si->value += si->value_step;
        if (*si->value >= si->value_max) *si->value = si->value_max;
        return;
    }
    if (astra_selector.selected_item->type == user_item &&
        astra_to_user_item(astra_selector.selected_item)->in_user_item)
        return;

    astra_refresh_list_value = true;
    uint8_t count = astra_selector.selected_item->parent->child_num;
    if (astra_selector.selected_index == count - 1) {
        astra_selector.selected_item  = astra_selector.selected_item->parent->child_list_item[0];
        astra_selector.selected_index = 0;
        return;
    }
    astra_selector.selected_item =
        astra_selector.selected_item->parent->child_list_item[++astra_selector.selected_index];
}

void astra_selector_go_prev_item(void)
{
    if (astra_selector.selected_item->type == slider_item &&
        astra_to_slider_item(astra_selector.selected_item)->is_confirmed) {
        astra_slider_item_t *si = astra_to_slider_item(astra_selector.selected_item);
        *si->value -= si->value_step;
        if (*si->value <= si->value_min) *si->value = si->value_min;
        return;
    }
    if (astra_selector.selected_item->type == user_item &&
        astra_to_user_item(astra_selector.selected_item)->in_user_item)
        return;

    astra_refresh_list_value = true;
    if (astra_selector.selected_index == 0) {
        uint8_t last = astra_selector.selected_item->parent->child_num - 1;
        astra_selector.selected_item  = astra_selector.selected_item->parent->child_list_item[last];
        astra_selector.selected_index = last;
        return;
    }
    astra_selector.selected_item =
        astra_selector.selected_item->parent->child_list_item[--astra_selector.selected_index];
}

bool astra_exit_animation_finished = true;

void astra_selector_jump_to_selected_item(void)
{
    if (!in_astra) return;

    if (astra_selector.selected_item->type == user_item) {
        astra_exit_animation_finished = false;
        astra_user_item_t *ui = astra_to_user_item(astra_selector.selected_item);
        ui->entering_user_item = true;
        ui->exiting_user_item  = false;
        ui->user_item_inited   = false;
        ui->user_item_looping  = false;
        return;
    }

    if (astra_selector.selected_item->type == switch_item) {
        astra_switch_item_t *si = astra_to_switch_item(astra_selector.selected_item);
        *si->value = !*si->value;
        if (si->exit_function) si->exit_function();
        return;
    }

    if (astra_selector.selected_item->type == button_item) {
        astra_button_item_t *bi = astra_to_button_item(astra_selector.selected_item);
        if (bi->exit_function) bi->exit_function();
        return;
    }

    if (astra_selector.selected_item->type == slider_item) {
        astra_slider_item_t *si = astra_to_slider_item(astra_selector.selected_item);
        if (!si->is_confirmed) {
            si->is_confirmed  = true;
            si->value_backup  = *si->value;
            return;
        }
        if (si->exit_function) si->exit_function();
        si->is_confirmed = false;
        return;
    }

    if (astra_selector.selected_item->child_num == 0) return;

    astra_refresh_list_value = true;
    for (uint8_t i = 0; i < astra_selector.selected_item->child_num; i++)
        astra_selector.selected_item->child_list_item[i]->y_list_item = 0;

    astra_selector.selected_index = 0;
    astra_selector.selected_item  = astra_selector.selected_item->child_list_item[0];
}

void astra_selector_exit_current_item(void)
{
    if (astra_selector.selected_item->type == slider_item &&
        astra_to_slider_item(astra_selector.selected_item)->is_confirmed) {
        astra_slider_item_t *si = astra_to_slider_item(astra_selector.selected_item);
        si->is_confirmed = false;
        *si->value = si->value_backup;
        return;
    }

    if (astra_selector.selected_item->type == user_item &&
        astra_to_user_item(astra_selector.selected_item)->in_user_item) {
        astra_exit_animation_finished = false;
        astra_user_item_t *ui = astra_to_user_item(astra_selector.selected_item);
        ui->entering_user_item = false;
        ui->exiting_user_item  = true;
        ui->user_item_inited   = false;
        ui->user_item_looping  = false;
        return;
    }

    astra_refresh_list_value = true;

    if (astra_selector.selected_item->parent->layer == 0 && in_astra) {
        if (ALLOW_EXIT_ASTRA_UI_BY_USER) in_astra = false;
        return;
    }

    for (uint8_t i = 0; i < astra_selector.selected_item->parent->parent->child_num; i++)
        astra_selector.selected_item->parent->parent->child_list_item[i]->y_list_item = 0;

    uint8_t idx = 0;
    for (uint8_t i = 0; i < astra_selector.selected_item->parent->parent->child_num; i++) {
        if (astra_selector.selected_item->parent->parent->child_list_item[i] ==
            astra_selector.selected_item->parent) {
            idx = i; break;
        }
    }
    astra_selector.selected_index = idx;
    astra_selector.selected_item  = astra_selector.selected_item->parent;
}

bool astra_push_item_to_list(astra_list_item_t *parent, astra_list_item_t *child)
{
    if (!parent || !child) return false;
    if (parent->child_num >= MAX_LIST_CHILD_NUM) return false;
    if (parent->layer >= MAX_LIST_LAYER) return false;

    child->layer     = parent->layer + 1;
    child->child_num = 0;

    astra_set_font(u8g2_font_my_chinese);
    if (parent->child_num == 0)
        child->y_list_item_trg = (float)oled_get_str_height() + LIST_FONT_TOP_MARGIN - 1;
    else
        child->y_list_item_trg =
            parent->child_list_item[parent->child_num - 1]->y_list_item_trg + LIST_ITEM_SPACING;

    if (parent->layer == 0 && parent->child_num == 0) {
        astra_bind_item_to_selector(child);
        astra_bind_selector_to_camera(&astra_selector);
    }

    parent->child_list_item[parent->child_num++] = child;
    child->parent = parent;
    return true;
}

/* ---- 相机 ---- */
astra_camera_t astra_camera = {0, 0, 0, 0, NULL};

astra_camera_t *astra_get_camera(void) { return &astra_camera; }

void astra_bind_selector_to_camera(astra_selector_t *sel)
{
    if (sel) astra_camera.selector = sel;
}
