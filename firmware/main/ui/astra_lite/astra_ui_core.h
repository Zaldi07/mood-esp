/* astra-lite core — 主循环与动画控制 */
#ifndef ASTRA_UI_CORE_H
#define ASTRA_UI_CORE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALLOW_EXIT_ASTRA_UI_BY_USER 1

extern bool in_astra;

bool astra_is_in_user_item(void);

void astra_refresh_info_bar(void);
void astra_refresh_pop_up(void);
void astra_refresh_camera_position(void);
void astra_refresh_widget_core_position(void);
void astra_init_list(void);
void astra_init_core(void);
void astra_refresh_list_item_position(void);
void astra_refresh_selector_position(void);
void astra_refresh_main_core_position(void);
void astra_ui_widget_core(void);
void astra_ui_main_core(void);

#ifdef __cplusplus
}
#endif

#endif /* ASTRA_UI_CORE_H */
