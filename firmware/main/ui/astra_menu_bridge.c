/* astra-lite 菜单桥接 — 纯 C 实现 */
#include "ui/astra_menu_bridge.h"

#include <stdio.h>

#include "app/time_sync.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "ui/carmood_ui.h"
#include "ui/astra_lite/astra_ui_core.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"
#include "ui/astra_lite/astra_ui_item.h"

static const char *TAG = "astra_menu";

static bool s_initialized;
static bool s_action_toggle_muyu;
static bool s_action_toggle_shooter;
static bool s_action_toggle_brick;
static bool s_action_toggle_flappy;
static bool s_action_run_calibration;
static bool s_debug_mode_value;

static void on_display_face_mode(void)
{
    carmood_ui_set_display_mode(CARMOOD_DISPLAY_MODE_FACE);
    astra_push_info_bar("表情模式", 800);
}

static void on_display_clock_mode(void)
{
    carmood_ui_set_display_mode(CARMOOD_DISPLAY_MODE_CLOCK);
    astra_push_info_bar("时钟模式", 800);
}

static void on_display_layout_mode(void)
{
    carmood_ui_set_display_mode(CARMOOD_DISPLAY_MODE_LAYOUT);
    astra_push_info_bar("布局页面", 800);
}

static void init_debug_mode_switch(void)
{
    s_debug_mode_value = carmood_ui_is_debug_mode();
}

static void on_debug_mode_changed(void)
{
    carmood_ui_set_debug_mode(s_debug_mode_value);
    astra_push_info_bar(s_debug_mode_value ? "DEBUG ON" : "DEBUG OFF", 800);
}

static void on_muyu_button(void)
{
    s_action_toggle_muyu = true;
}

static void on_shooter_button(void)
{
    s_action_toggle_shooter = true;
}

static void on_brick_button(void)
{
    s_action_toggle_brick = true;
}

static void on_flappy_button(void)
{
    s_action_toggle_flappy = true;
}

static void on_calibration_button(void)
{
    s_action_run_calibration = true;
}

static void astra_status_page_loop(void)
{
    char turn_char = 'C';
    char pitch_char = 'N';
    bool muyu_mode = false;
    carmood_display_mode_t display_mode = carmood_ui_get_display_mode();
    bool debug_mode = carmood_ui_is_debug_mode();
    char line[24];
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    uint32_t uptime_m = uptime_s / 60U;
    uint32_t heap_kb = esp_get_free_heap_size() / 1024U;

    carmood_ui_get_status(NULL, &turn_char, &pitch_char, &muyu_mode, NULL);

    astra_set_font(u8g2_font_my_chinese);
    oled_set_draw_color(1);

    oled_draw_UTF8(2, 10, "系统状态");
    oled_draw_H_line(0, 13, OLED_WIDTH - 8);

    snprintf(line, sizeof(line), "模式:%s", muyu_mode ? "木鱼" : "正常");
    oled_draw_UTF8(2, 24, line);

    snprintf(line, sizeof(line), "时间:%s", carmood_time_sync_is_synced() ? "已同步" : "未同步");
    oled_draw_UTF8(2, 34, line);

    snprintf(line, sizeof(line), "显示:%s",
             display_mode == CARMOOD_DISPLAY_MODE_FACE ? "表情" :
             (display_mode == CARMOOD_DISPLAY_MODE_CLOCK ? "时钟" : "布局"));
    oled_draw_UTF8(2, 44, line);

    snprintf(line, sizeof(line), "Debug:%s", debug_mode ? "ON" : "OFF");
    oled_draw_UTF8(2, 56, line);

    snprintf(line, sizeof(line), "方向:%c/%c", turn_char, pitch_char);
    oled_draw_UTF8(66, 24, line);

    snprintf(line, sizeof(line), "内存:%luK", (unsigned long)heap_kb);
    oled_draw_UTF8(66, 34, line);

    snprintf(line, sizeof(line), "Run:%lum", (unsigned long)uptime_m);
    oled_draw_UTF8(60, 44, line);

    oled_draw_UTF8(50, 56, "长按顶部返回");
}

void astra_menu_init(void)
{
    if (s_initialized) return;

    astra_ui_driver_init();

    /* 构建菜单树 */
    astra_list_item_t *root = astra_get_root_list();
    astra_list_item_t *game_menu = astra_new_list_item("游戏选择", list_icon);
    astra_list_item_t *display_menu = astra_new_list_item("显示模式", list_icon);
    astra_push_item_to_list(root, game_menu);
    astra_push_item_to_list(root,
        astra_new_button_item("方向标定", on_calibration_button, flag_icon));
    astra_push_item_to_list(root, display_menu);
    astra_push_item_to_list(root,
        astra_new_switch_item("DEBUG模式", &s_debug_mode_value,
                              init_debug_mode_switch, on_debug_mode_changed,
                              switch_icon));
    astra_push_item_to_list(root,
        astra_new_user_item("系统状态", NULL, astra_status_page_loop, NULL, list_icon));
    astra_push_item_to_list(game_menu,
        astra_new_button_item("功德木鱼", on_muyu_button, flag_icon));
    astra_push_item_to_list(game_menu,
        astra_new_button_item("打砖块", on_brick_button, flag_icon));
    astra_push_item_to_list(game_menu,
        astra_new_button_item("像素小鸟", on_flappy_button, flag_icon));
    astra_push_item_to_list(game_menu,
        astra_new_button_item("打飞机", on_shooter_button, flag_icon));
    astra_push_item_to_list(display_menu,
        astra_new_button_item("表情模式", on_display_face_mode, flag_icon));
    astra_push_item_to_list(display_menu,
        astra_new_button_item("时钟模式", on_display_clock_mode, flag_icon));
    astra_push_item_to_list(display_menu,
        astra_new_button_item("布局页面", on_display_layout_mode, flag_icon));

    astra_init_core();

    s_initialized = true;
    ESP_LOGI(TAG, "Astra-Lite 菜单初始化完成");
}

void astra_menu_open(void)
{
    if (!s_initialized) astra_menu_init();

    s_action_toggle_muyu      = false;
    s_action_toggle_shooter   = false;
    s_action_toggle_brick     = false;
    s_action_toggle_flappy    = false;
    s_action_run_calibration  = false;

    /* 重置选择状态 */
    astra_init_list();
    in_astra = true;
}

void astra_menu_close(void)
{
    in_astra = false;
}

bool astra_menu_is_open(void)
{
    return in_astra;
}

void astra_menu_tick(void)
{
    if (!in_astra) return;

    oled_clear_buffer();
    astra_ui_main_core();
    astra_ui_widget_core();
    oled_send_buffer();
}

void astra_menu_input_up(void)
{
    if (!in_astra) return;
    astra_selector_go_prev_item();
}

void astra_menu_input_down(void)
{
    if (!in_astra) return;
    astra_selector_go_next_item();
}

void astra_menu_input_ok(void)
{
    if (!in_astra) return;
    astra_selector_jump_to_selected_item();
}

void astra_menu_input_back(void)
{
    if (!in_astra) return;
    astra_selector_exit_current_item();
}

bool astra_menu_should_run_calibration(void)
{
    return s_action_run_calibration;
}

bool astra_menu_should_toggle_muyu(void)
{
    return s_action_toggle_muyu;
}

bool astra_menu_should_toggle_shooter(void)
{
    return s_action_toggle_shooter;
}

bool astra_menu_should_toggle_brick(void)
{
    return s_action_toggle_brick;
}

bool astra_menu_should_toggle_flappy(void)
{
    return s_action_toggle_flappy;
}

void astra_menu_consume_actions(void)
{
    s_action_toggle_muyu     = false;
    s_action_toggle_shooter  = false;
    s_action_toggle_brick    = false;
    s_action_toggle_flappy   = false;
    s_action_run_calibration = false;
}
