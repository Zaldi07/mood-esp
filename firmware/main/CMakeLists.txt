file(GLOB U8G2_SRCS "libs/u8g2/*.c")

idf_component_register(
    SRCS
        "main.c"
        "app/carmood_app.c"
        "app/time_sync.c"
        "ssd1306_oled.c"
        "ui/carmood_ui.c"
        "ui/layout_renderer.c"
        "ui/face_renderer.c"
        "ui/muyu_renderer.c"
        "ui/brick_game.c"
        "ui/flappy_game.c"
        "ui/shooter_game.c"
        "ui/astra_menu_bridge.c"
        "ui/astra_lite/astra_ui_draw_driver.c"
        "ui/astra_lite/astra_ui_item.c"
        "ui/astra_lite/astra_ui_core.c"
        "ui/astra_lite/astra_ui_drawer.c"
        "ui/astra_lite/Others/u8g2_font_my_chinese.c"
        "drivers/lis3dh.c"
        "drivers/touch_input.c"
        ${U8G2_SRCS}
    INCLUDE_DIRS "."
)
