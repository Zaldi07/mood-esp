/* astra-lite item 定义 — 列表项 / 选择器 / 相机 */
#ifndef ASTRA_UI_ITEM_H
#define ASTRA_UI_ITEM_H

#include "ui/astra_lite/astra_ui_draw_driver.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void astra_set_font(const void *_font);

extern bool astra_exit_animation_finished;
extern bool astra_refresh_list_value;

/* ---- 信息栏 ---- */
#define INFO_BAR_HEIGHT 15
#define INFO_BAR_OFFSET 10

typedef struct {
    char    *content;
    uint16_t span;
    float    y_info_bar, y_info_bar_trg, w_info_bar, w_info_bar_trg;
    bool     is_running;
    uint32_t time_start;
    uint32_t time;
} astra_info_bar_t;

extern astra_info_bar_t astra_info_bar;
extern void astra_push_info_bar(char *_content, uint16_t _span);

/* ---- 弹窗 ---- */
#define POP_UP_HEIGHT 20
#define POP_UP_OFFSET 8

typedef struct {
    char    *content;
    uint16_t span;
    float    y_pop_up, y_pop_up_trg, w_pop_up, w_pop_up_trg;
    bool     is_running;
    uint32_t time_start;
    uint32_t time;
} astra_pop_up_t;

extern astra_pop_up_t astra_pop_up;
extern void astra_push_pop_up(char *_content, uint16_t _span);

/* ---- 列表项 ---- */
#define MAX_LIST_CHILD_NUM  10
#define MAX_LIST_LAYER      10
#define SCREEN_HEIGHT       64
#define SCREEN_WIDTH        128
#define LIST_ITEM_SPACING      15
#define LIST_ITEM_OFFSET       8
#define LIST_ITEM_LEFT_MARGIN  4
#define LIST_ITEM_RIGHT_MARGIN 20
#define LIST_INFO_BAR_HEIGHT   3
#define LIST_FONT_TOP_MARGIN   4
#define LIST_TEXT_OFFSET_X     3

typedef enum {
    list_item,
    switch_item,
    slider_item,
    user_item,
    button_item,
} astra_list_item_type_t;

typedef enum {
    default_icon,
    list_icon,
    switch_icon,
    plus_icon,
    user_icon,
    slider_icon,
    flag_icon,
    power_icon,
} astra_list_item_icon_t;

typedef struct astra_list_item_t {
    astra_list_item_type_t  type;
    astra_list_item_icon_t  icon;
    char *content;

    uint8_t layer;
    float   y_list_item, y_list_item_trg;
    uint8_t child_num;
    struct astra_list_item_t *child_list_item[MAX_LIST_CHILD_NUM];
    struct astra_list_item_t *parent;
} astra_list_item_t;

typedef struct {
    astra_list_item_t base_item;
    bool *value;
    void (*init_function)(void);
    void (*exit_function)(void);
} astra_switch_item_t;

typedef struct {
    astra_list_item_t base_item;
    void (*exit_function)(void);
} astra_button_item_t;

typedef struct {
    astra_list_item_t base_item;
    int16_t *value;
    int16_t  value_backup;
    bool     is_confirmed;
    uint8_t  value_step;
    int16_t  value_max;
    int16_t  value_min;
    void (*init_function)(void);
    void (*exit_function)(void);
} astra_slider_item_t;

typedef struct {
    astra_list_item_t base_item;
    bool in_user_item;
    bool entering_user_item;
    bool exiting_user_item;
    void (*init_function)(void);
    void (*loop_function)(void);
    void (*exit_function)(void);
    bool user_item_inited;
    bool user_item_looping;
} astra_user_item_t;

astra_list_item_t *astra_get_root_list(void);

astra_switch_item_t *astra_to_switch_item(astra_list_item_t *item);
astra_button_item_t *astra_to_button_item(astra_list_item_t *item);
astra_slider_item_t *astra_to_slider_item(astra_list_item_t *item);
astra_user_item_t   *astra_to_user_item(astra_list_item_t *item);

astra_list_item_t *astra_new_list_item(char *content, astra_list_item_icon_t icon);
astra_list_item_t *astra_new_switch_item(char *content, bool *value,
                                         void (*init_fn)(void), void (*exit_fn)(void),
                                         astra_list_item_icon_t icon);
astra_list_item_t *astra_new_button_item(char *content, void (*exit_fn)(void),
                                         astra_list_item_icon_t icon);
astra_list_item_t *astra_new_slider_item(char *content, int16_t *value,
                                         uint8_t step, int16_t min, int16_t max,
                                         void (*init_fn)(void), void (*exit_fn)(void),
                                         astra_list_item_icon_t icon);
astra_list_item_t *astra_new_user_item(char *content,
                                       void (*init_fn)(void), void (*loop_fn)(void),
                                       void (*exit_fn)(void),
                                       astra_list_item_icon_t icon);

bool astra_push_item_to_list(astra_list_item_t *parent, astra_list_item_t *child);

/* ---- 选择器 ---- */
typedef struct {
    float   y_selector, y_selector_trg;
    float   w_selector, w_selector_trg;
    float   h_selector, h_selector_trg;
    uint8_t selected_index;
    astra_list_item_t *selected_item;
} astra_selector_t;

extern astra_selector_t astra_selector;
astra_selector_t *astra_get_selector(void);
bool astra_bind_item_to_selector(astra_list_item_t *item);
void astra_selector_go_next_item(void);
void astra_selector_go_prev_item(void);
void astra_selector_jump_to_selected_item(void);
void astra_selector_exit_current_item(void);

/* ---- 相机 ---- */
typedef struct {
    float x_camera, x_camera_trg;
    float y_camera, y_camera_trg;
    astra_selector_t *selector;
} astra_camera_t;

extern astra_camera_t astra_camera;
astra_camera_t *astra_get_camera(void);
void astra_bind_selector_to_camera(astra_selector_t *selector);

#ifdef __cplusplus
}
#endif

#endif /* ASTRA_UI_ITEM_H */
