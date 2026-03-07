/* astra-lite 菜单桥接 — C 接口 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void astra_menu_init(void);
void astra_menu_open(void);
void astra_menu_close(void);
bool astra_menu_is_open(void);
void astra_menu_tick(void);

void astra_menu_input_up(void);
void astra_menu_input_down(void);
void astra_menu_input_ok(void);
void astra_menu_input_back(void);

bool astra_menu_should_run_calibration(void);
bool astra_menu_should_toggle_muyu(void);
bool astra_menu_should_toggle_shooter(void);
bool astra_menu_should_toggle_brick(void);
bool astra_menu_should_toggle_flappy(void);
void astra_menu_consume_actions(void);

#ifdef __cplusplus
}
#endif
