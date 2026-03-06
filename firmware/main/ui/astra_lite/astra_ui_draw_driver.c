/* astra-lite draw driver — ESP32 + u8g2 + SSD1306 实现 */
#include "ui/astra_lite/astra_ui_draw_driver.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

u8g2_t g_astra_u8g2;

static uint8_t byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8; (void)msg; (void)arg_int; (void)arg_ptr;
    return 1;
}

static uint8_t gpio_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8; (void)arg_ptr;
    if (msg == U8X8_MSG_DELAY_MILLI)
        vTaskDelay(pdMS_TO_TICKS(arg_int == 0 ? 1 : arg_int));
    return 1;
}

uint32_t astra_get_ticks(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void astra_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms == 0 ? 1 : ms));
}

void astra_ui_driver_init(void)
{
    u8g2_Setup_ssd1306_128x64_noname_f(&g_astra_u8g2, U8G2_R0, byte_cb, gpio_delay_cb);
    u8g2_InitDisplay(&g_astra_u8g2);
    u8g2_SetPowerSave(&g_astra_u8g2, 0);
    u8g2_ClearBuffer(&g_astra_u8g2);
    u8g2_SetFontMode(&g_astra_u8g2, 1);
    u8g2_SetFontDirection(&g_astra_u8g2, 0);
    u8g2_SetFont(&g_astra_u8g2, u8g2_font_my_chinese);
}
