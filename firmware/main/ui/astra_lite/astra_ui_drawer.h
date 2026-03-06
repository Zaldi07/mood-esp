/* astra-lite drawer — 渲染相关声明 */
#ifndef ASTRA_UI_DRAWER_H
#define ASTRA_UI_DRAWER_H

#include "ui/astra_lite/astra_ui_item.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t astra_exit_animation_status;

void astra_draw_exit_animation(void);
void astra_draw_info_bar(void);
void astra_draw_pop_up(void);
void astra_draw_list_appearance(void);
void astra_draw_list_item(void);
void astra_draw_list_icon(astra_list_item_icon_t icon, uint16_t x, uint16_t y);
void astra_draw_selector(void);
void astra_draw_widget(void);
void astra_draw_list(void);

#ifdef __cplusplus
}
#endif

#endif /* ASTRA_UI_DRAWER_H */
