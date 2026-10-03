/* astra-lite drawer 实现 */
#include "ui/astra_lite/astra_ui_drawer.h"
#include "ui/astra_lite/astra_ui_core.h"

#include <math.h>
#include <stdio.h>
static void astra_exit_anim_interp(float *pos, float trg, float speed)
{
    if (*pos != trg) {
        if (fabsf(*pos - trg) <= 1.0f) *pos = trg;
        else *pos += (trg - *pos) / ((100.0f - speed) / 1.0f);
    }
}

uint8_t astra_exit_animation_status = 0;

void astra_draw_exit_animation(void)
{
    static float h     = -8;
    static float h_trg = OLED_HEIGHT + 8;

    /*
     * 阶段0：黑幕+沙漏从上往下覆盖旧内容
     * 阶段1（覆盖完成）：直接结束动画，不再做收回阶段
     *
     * 原来阶段1之后会进入阶段2让黑幕慢慢收回，这段时间新内容虽然已经
     * 绘制，但仍被黑色遮罩覆盖，导致用户看到明显的黑屏闪烁。
     * 改为覆盖完成后直接结束，新内容下一帧立即可见。
     */

    /* 阶段1：黑幕已完全覆盖，跳过绘制直接结束 */
    if (astra_exit_animation_status == 1) {
        astra_exit_animation_finished = true;
        astra_exit_animation_status = 0;
        h = -8;
        h_trg = OLED_HEIGHT + 8;
        return;
    }

    oled_set_draw_color(0);
    oled_draw_box(0, 0, OLED_WIDTH, (int16_t)h);
    oled_set_draw_color(1);

    /* 沙漏图案 */
    uint8_t xo = OLED_WIDTH / 2 - 8;
    int8_t  yh = (int8_t)(h - OLED_HEIGHT / 2 - 18);
    if (yh + 20 >= 0) {
        oled_draw_box(xo, yh + 2, 13, 3);
        oled_set_draw_color(0);
        oled_draw_H_line(xo + 2, yh + 3, 9);
        oled_set_draw_color(1);

        oled_draw_V_line(xo + 1, yh + 4, 5);
        oled_draw_V_line(xo + 11, yh + 4, 5);

        for (uint8_t i = 0; i < 5; ++i) {
            int8_t cy = yh + 8 + i;
            int8_t lx = (i < 3) ? (xo + 1 + i) : (xo + 4);
            int8_t rx = (i < 3) ? (xo + 10 - i) : (xo + 7);
            oled_draw_H_line(lx, cy, 2);
            oled_draw_H_line(rx, cy, 2);
        }

        for (uint8_t i = 0; i < 3; ++i) {
            int8_t cy = yh + 13 + i;
            oled_draw_H_line(xo + 3 - i, cy, 2);
            oled_draw_H_line(xo + 8 + i, cy, 2);
        }

        oled_draw_V_line(xo + 1, yh + 16, 3);
        oled_draw_V_line(xo + 11, yh + 16, 3);

        oled_draw_box(xo, yh + 19, 13, 3);
        oled_set_draw_color(0);
        oled_draw_H_line(xo + 2, yh + 20, 9);
        oled_set_draw_color(1);

        static const uint8_t pts[][2] = {
            {5,7},{7,7},{6,8},{6,10},{6,14},{6,16},
            {5,17},{7,17},{4,18},{6,18},{8,18}
        };
        for (uint8_t i = 0; i < sizeof(pts)/sizeof(pts[0]); ++i)
            oled_draw_pixel(xo + pts[i][0], yh + pts[i][1]);
    }

    if (h + 3 >= 0)
        for (uint8_t i = 0; i <= 3; ++i)
            oled_draw_H_line(0, (int16_t)(h + i), OLED_WIDTH);

    /* 棋盘格过渡 */
    for (int16_t i = 0; i <= OLED_WIDTH; i += 2)
        for (int16_t j = (int16_t)(h - 5); j <= (int16_t)(h - 1); j++) {
            if (j % 2 == 0) oled_draw_pixel(i + 1, j);
            if (j % 2 == 1) oled_draw_pixel(i, j);
        }

    astra_exit_anim_interp(&h, h_trg, 94);

    if (astra_exit_animation_status == 0 && h == h_trg && h_trg == OLED_HEIGHT + 8) {
        astra_exit_animation_status = 1;
        return;
    }
}

void astra_draw_info_bar(void)
{
    if (!astra_info_bar.is_running) return;

    if (astra_info_bar.y_info_bar == astra_info_bar.y_info_bar_trg)
        astra_info_bar.time = get_ticks();

    if (astra_info_bar.time - astra_info_bar.time_start >= astra_info_bar.span) {
        astra_info_bar.y_info_bar_trg = -2.0f * INFO_BAR_HEIGHT;
        if (astra_info_bar.y_info_bar == astra_info_bar.y_info_bar_trg)
            astra_info_bar.is_running = false;
    }

    int16_t xb  = OLED_WIDTH / 2 - (int16_t)(astra_info_bar.w_info_bar / 2);
    int16_t yb1 = (int16_t)(astra_info_bar.y_info_bar - 4);

    astra_set_font(u8g2_font_my_chinese);
    oled_set_draw_color(1);
    oled_draw_R_box(xb + 3, yb1 + 3,
                    (int16_t)astra_info_bar.w_info_bar, INFO_BAR_HEIGHT + 4, 4);

    oled_set_draw_color(0);
    oled_draw_R_box((int16_t)(OLED_WIDTH / 2 - (astra_info_bar.w_info_bar + 4) / 2), yb1,
                    (int16_t)(astra_info_bar.w_info_bar + 4), INFO_BAR_HEIGHT + 6, 4);

    oled_set_draw_color(1);
    oled_draw_R_box(xb, yb1, (int16_t)astra_info_bar.w_info_bar, INFO_BAR_HEIGHT + 4, 3);

    oled_set_draw_color(0);
    oled_draw_H_line(xb + 2, (int16_t)(astra_info_bar.y_info_bar + INFO_BAR_HEIGHT - 2),
                     (int16_t)(astra_info_bar.w_info_bar - 4));

    oled_draw_UTF8(xb + 6,
                   (int16_t)(astra_info_bar.y_info_bar + oled_get_str_height() - 2),
                   astra_info_bar.content);
}

void astra_draw_pop_up(void)
{
    if (!astra_pop_up.is_running) return;

    if (astra_pop_up.y_pop_up == astra_pop_up.y_pop_up_trg)
        astra_pop_up.time = get_ticks();

    if (astra_pop_up.time - astra_pop_up.time_start >= astra_pop_up.span) {
        astra_pop_up.y_pop_up_trg = -2.0f * POP_UP_HEIGHT;
        if (astra_pop_up.y_pop_up == astra_pop_up.y_pop_up_trg)
            astra_pop_up.is_running = false;
    }

    int16_t xp = OLED_WIDTH / 2 - (int16_t)(astra_pop_up.w_pop_up / 2);

    astra_set_font(u8g2_font_my_chinese);
    oled_set_draw_color(1);
    oled_draw_R_box(xp + 1, (int16_t)(astra_pop_up.y_pop_up + 3),
                    (int16_t)(astra_pop_up.w_pop_up + 4), POP_UP_HEIGHT, 4);

    oled_set_draw_color(0);
    oled_draw_R_box((int16_t)(OLED_WIDTH / 2 - (astra_pop_up.w_pop_up + 4) / 2 - 2),
                    (int16_t)(astra_pop_up.y_pop_up - 2),
                    (int16_t)(astra_pop_up.w_pop_up + 8), POP_UP_HEIGHT + 4, 5);

    oled_set_draw_color(1);
    oled_draw_R_box(xp - 2, (int16_t)astra_pop_up.y_pop_up,
                    (int16_t)(astra_pop_up.w_pop_up + 4), POP_UP_HEIGHT, 3);

    oled_set_draw_color(0);
    oled_draw_H_line(xp, (int16_t)(astra_pop_up.y_pop_up + POP_UP_HEIGHT - 2),
                     (int16_t)astra_pop_up.w_pop_up);

    oled_draw_UTF8(xp + 3,
                   (int16_t)(astra_pop_up.y_pop_up + oled_get_str_height() + 1),
                   astra_pop_up.content);
}

void astra_draw_list_appearance(void)
{
    oled_set_draw_color(1);

    /* 顶部状态栏装饰线 */
    oled_draw_H_line(0, 1, 66);
    oled_draw_H_line(0, 0, 67);

    static const struct { uint8_t s, e, step, y; } dcfg[] = {
        {67,99,2,1}, {68,100,2,0}, {102,111,3,1}, {103,112,3,0},
        {115,124,5,1}, {116,124,5,0}
    };
    for (uint8_t j = 0; j < sizeof(dcfg)/sizeof(dcfg[0]); ++j)
        for (uint8_t i = dcfg[j].s; i <= dcfg[j].e; i += dcfg[j].step)
            oled_draw_pixel(i, dcfg[j].y);

    /* 右侧进度条 */
    oled_draw_V_line(OLED_WIDTH - 5, 0, OLED_HEIGHT);
    oled_draw_V_line(OLED_WIDTH - 1, 0, OLED_HEIGHT);

    /* 滑块 */
    uint8_t cnt = astra_selector.selected_item->parent->child_num;
    float len = ceilf((SCREEN_HEIGHT - 10.0f) / (float)cnt);
    oled_draw_box(OLED_WIDTH - 4,
                  (int16_t)(5 + astra_selector.selected_index * len), 3, (int16_t)len);

    oled_set_draw_color(0);
    oled_draw_H_line(OLED_WIDTH - 4,
                     (int16_t)(len + (float)astra_selector.selected_index * len), 3);
    if (len >= 9) {
        oled_draw_H_line(OLED_WIDTH - 4,
                         (int16_t)(floorf(len - 2.0f + (float)astra_selector.selected_index * len)), 3);
        oled_draw_H_line(OLED_WIDTH - 4,
                         (int16_t)(floorf(len + 2.0f + (float)astra_selector.selected_index * len)), 3);
    }

    oled_set_draw_color(1);
    oled_draw_box(OLED_WIDTH - 4, 0, 3, 4);
    oled_draw_box(OLED_WIDTH - 4, OLED_HEIGHT - 4, 3, 4);
    oled_set_draw_color(0);
    oled_draw_H_line(OLED_WIDTH - 4, 2, 3);
    oled_draw_pixel(OLED_WIDTH - 3, 1);
    oled_draw_H_line(OLED_WIDTH - 4, OLED_HEIGHT - 3, 3);
    oled_draw_pixel(OLED_WIDTH - 3, OLED_HEIGHT - 2);
}

void astra_draw_list_icon(astra_list_item_icon_t icon, uint16_t x, uint16_t y)
{
    (void)icon;
    (void)x;
    (void)y;
    /* Left list icon is removed/emptied per user request */
}

void astra_draw_list_item(void)
{
    astra_list_item_t *parent = astra_selector.selected_item->parent;
    int16_t sh = oled_get_str_height();


    for (uint8_t i = 0; i < parent->child_num; i++) {
        int16_t xi = (int16_t)(astra_camera.x_camera + LIST_ITEM_LEFT_MARGIN);
        int16_t yi = (int16_t)(parent->child_list_item[i]->y_list_item +
                               astra_camera.y_camera - sh / 2);
        int16_t text_y = yi + sh / 2;

        oled_set_draw_color(1);

        astra_list_item_t *ci = parent->child_list_item[i];

        if (ci->type == switch_item) {
            astra_switch_item_t *sw = astra_to_switch_item(ci);
            if (sw->init_function && astra_refresh_list_value)
                sw->init_function();
            if (yi + 7 > LIST_INFO_BAR_HEIGHT && yi + 1 < SCREEN_HEIGHT) {
                astra_draw_list_icon(ci->icon, xi, yi);
                oled_draw_frame(OLED_WIDTH - LIST_ITEM_RIGHT_MARGIN - 7, yi - 2, 11, 7);
                if (*sw->value) {
                    oled_draw_box(OLED_WIDTH - LIST_ITEM_RIGHT_MARGIN - 1, yi, 3, 3);
                    oled_draw_pixel(OLED_WIDTH - LIST_ITEM_RIGHT_MARGIN - 4, yi + 1);
                } else {
                    oled_draw_box(OLED_WIDTH - LIST_ITEM_RIGHT_MARGIN - 5, yi, 3, 3);
                    oled_draw_pixel(OLED_WIDTH - LIST_ITEM_RIGHT_MARGIN, yi + 1);
                }
            }
        } else if (ci->type == slider_item) {
            astra_slider_item_t *sl = astra_to_slider_item(ci);
            if (sl->init_function && astra_refresh_list_value)
                sl->init_function();
            if (yi + 5 > LIST_INFO_BAR_HEIGHT && yi - 2 < SCREEN_HEIGHT) {
                astra_draw_list_icon(ci->icon, xi, yi);
                char vstr[10];
                snprintf(vstr, sizeof(vstr), "%d", *sl->value);
                int16_t xv = OLED_WIDTH - LIST_ITEM_RIGHT_MARGIN -
                             (int16_t)oled_get_str_width(vstr) + 2;
                if (sl->is_confirmed) {
                    static uint32_t ltk = 0;
                    static bool vis = false;
                    uint32_t tk = get_ticks();
                    if (vis) {
                        oled_set_draw_color(1);
                        oled_draw_R_box(xv, yi - 4,
                                        oled_get_UTF8_width(vstr) + 4, sh - 1, 1);
                    }
                    oled_set_draw_color(0);
                    oled_draw_str(xv + 2, text_y, vstr);
                    if (tk - ltk >= 1000) { vis = !vis; ltk = tk; }
                } else {
                    oled_draw_str(xv + 2, text_y, vstr);
                }
            }
        } else if (ci->type == button_item) {
            if (yi + 7 > LIST_INFO_BAR_HEIGHT && yi + 1 < SCREEN_HEIGHT)
                astra_draw_list_icon(ci->icon, xi, yi);
        } else {
            if (yi + 2 > LIST_INFO_BAR_HEIGHT && yi - 2 < SCREEN_HEIGHT)
                astra_draw_list_icon(ci->icon, xi, yi);
        }

        astra_set_font(u8g2_font_my_chinese);
        if (text_y > LIST_INFO_BAR_HEIGHT && text_y < SCREEN_HEIGHT)
            oled_draw_UTF8(LIST_TEXT_OFFSET_X + xi, text_y, ci->content);
    }

    astra_refresh_list_value = false;
}

void astra_draw_selector(void)
{
    int16_t xs = (int16_t)(astra_camera.x_camera + LIST_ITEM_LEFT_MARGIN);
    int16_t ys = (int16_t)(astra_selector.y_selector + astra_camera.y_camera);

    oled_set_draw_color(2);
    oled_draw_box(xs, ys,
                  (int16_t)astra_selector.w_selector, (int16_t)astra_selector.h_selector);

    /* 棋盘格渐变边缘 */
    oled_set_draw_color(1);
    for (int16_t i = (int16_t)(astra_selector.w_selector + xs);
         i <= (int16_t)(astra_selector.w_selector + xs + 7); i += 2) {
        for (int16_t j = ys; j <= ys + (int16_t)astra_selector.h_selector - 1; j++) {
            if (j % 2 == 0) oled_draw_pixel(i + 1, j);
            if (j % 2 == 1) oled_draw_pixel(i, j);
        }
    }
}

void astra_draw_widget(void)
{
    astra_draw_info_bar();
    astra_draw_pop_up();
}

void astra_draw_list(void)
{
    astra_draw_list_appearance();
    astra_draw_list_item();
    astra_draw_selector();
}
