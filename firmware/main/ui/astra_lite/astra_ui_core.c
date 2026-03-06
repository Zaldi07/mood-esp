/* astra-lite core 实现 */
#include "ui/astra_lite/astra_ui_core.h"
#include "ui/astra_lite/astra_ui_drawer.h"
#include "ui/astra_lite/astra_ui_item.h"

#include <math.h>

bool in_astra = false;

bool astra_is_in_user_item(void)
{
    return (astra_selector.selected_item->type == user_item &&
            astra_to_user_item(astra_selector.selected_item)->in_user_item);
}

static void astra_animation(float *pos, float trg, float speed)
{
    if (*pos != trg) {
        if (fabsf(*pos - trg) <= 1.0f)
            *pos = trg;
        else
            *pos += (trg - *pos) / ((100.0f - speed) / 1.0f);
    }
}

void astra_refresh_info_bar(void)
{
    astra_animation(&astra_info_bar.y_info_bar, astra_info_bar.y_info_bar_trg, 94);
    astra_animation(&astra_info_bar.w_info_bar, astra_info_bar.w_info_bar_trg, 95);
}

void astra_refresh_pop_up(void)
{
    astra_animation(&astra_pop_up.y_pop_up, astra_pop_up.y_pop_up_trg, 94);
    astra_animation(&astra_pop_up.w_pop_up, astra_pop_up.w_pop_up_trg, 96);
}

void astra_refresh_camera_position(void)
{
    if (astra_camera.selector->y_selector_trg + 15 + astra_camera.y_camera_trg > SCREEN_HEIGHT)
        astra_camera.y_camera_trg = SCREEN_HEIGHT - astra_camera.selector->y_selector_trg - 15;

    if (astra_camera.selector->y_selector_trg + astra_camera.y_camera_trg < 0)
        astra_camera.y_camera_trg = 0 - astra_camera.selector->y_selector_trg + LIST_FONT_TOP_MARGIN;

    astra_animation(&astra_camera.x_camera, astra_camera.x_camera_trg, 96);
    astra_animation(&astra_camera.y_camera, astra_camera.y_camera_trg, 96);
}

void astra_refresh_widget_core_position(void)
{
    astra_refresh_info_bar();
    astra_refresh_pop_up();
}

void astra_init_list(void)
{
    for (uint8_t i = 0; i < astra_get_root_list()->child_num; i++)
        astra_get_root_list()->child_list_item[i]->y_list_item = 0;

    astra_selector.selected_index = 0;
    astra_selector.selected_item  = astra_get_root_list()->child_list_item[0];
    astra_selector.y_selector     = OLED_HEIGHT;
    astra_selector.h_selector     = OLED_HEIGHT;
}

void astra_init_core(void)
{
    astra_init_list();
    astra_bind_item_to_selector(astra_get_root_list()->child_list_item[0]);
    astra_bind_selector_to_camera(astra_get_selector());
}

void astra_refresh_list_item_position(void)
{
    astra_list_item_t *parent = astra_selector.selected_item->parent;
    for (uint8_t i = 0; i < parent->child_num; i++)
        astra_animation(&parent->child_list_item[i]->y_list_item,
                        parent->child_list_item[i]->y_list_item_trg, 84);
}

void astra_refresh_selector_position(void)
{
    astra_set_font(u8g2_font_my_chinese);
    astra_selector.y_selector_trg =
        astra_selector.selected_item->y_list_item_trg - (float)oled_get_str_height() + 1;

    if (astra_selector.selected_item->type == switch_item ||
        astra_selector.selected_item->type == slider_item)
        astra_selector.w_selector_trg = OLED_WIDTH - 18;
    else
        astra_selector.w_selector_trg =
            (float)oled_get_UTF8_width(astra_selector.selected_item->content) + 12;

    astra_selector.h_selector_trg = 15;
    astra_animation(&astra_selector.y_selector, astra_selector.y_selector_trg, 91);
    astra_animation(&astra_selector.w_selector, astra_selector.w_selector_trg, 92);
    astra_animation(&astra_selector.h_selector, astra_selector.h_selector_trg, 93);
}

void astra_refresh_main_core_position(void)
{
    astra_refresh_list_item_position();
}

void astra_ui_widget_core(void)
{
    astra_refresh_widget_core_position();
    astra_draw_widget();
}

void astra_ui_main_core(void)
{
    if (!in_astra) return;

    /* user_item 进入逻辑 */
    if (astra_selector.selected_item->type == user_item &&
        !astra_to_user_item(astra_selector.selected_item)->in_user_item) {
        astra_user_item_t *ui = astra_to_user_item(astra_selector.selected_item);
        if (ui->entering_user_item && astra_exit_animation_status == 1) {
            if (ui->init_function) ui->init_function();
            ui->in_user_item = 1;
        }
    }

    /* 渲染逻辑 */
    if (astra_selector.selected_item->type == user_item &&
        astra_to_user_item(astra_selector.selected_item)->in_user_item) {
        astra_user_item_t *ui = astra_to_user_item(astra_selector.selected_item);
        if (ui->loop_function) ui->loop_function();

        if (ui->exiting_user_item && astra_exit_animation_status == 1) {
            if (ui->exit_function) ui->exit_function();
            ui->in_user_item = 0;
        }
    } else {
        astra_refresh_camera_position();
        astra_refresh_main_core_position();
        astra_refresh_selector_position();
        astra_draw_list();
    }

    /* 退场动画 */
    if (!astra_exit_animation_finished)
        astra_draw_exit_animation();
}
