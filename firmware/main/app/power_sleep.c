#include "app/power_sleep.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "drivers/buzzer.h"
#include "ssd1306_oled.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"

static const char *TAG = "power_sleep";

#ifndef CONFIG_CARMOOD_TOUCH_GPIO
#define CONFIG_CARMOOD_TOUCH_GPIO 7
#endif

void power_sleep_draw_countdown(int64_t hold_ms) {
    if (hold_ms < POWER_SLEEP_HOLD_THRESHOLD_MS) {
        return;
    }

    oled_clear_buffer();
    oled_set_font(u8g2_font_my_chinese);

    int remaining_sec = (int)((POWER_SLEEP_HOLD_TRIGGER_MS - hold_ms + 999) / 1000);
    if (remaining_sec < 1) remaining_sec = 1;

    int progress_pct = (int)((hold_ms - POWER_SLEEP_HOLD_THRESHOLD_MS) * 100 /
                             (POWER_SLEEP_HOLD_TRIGGER_MS - POWER_SLEEP_HOLD_THRESHOLD_MS));
    if (progress_pct > 100) progress_pct = 100;
    if (progress_pct < 0) progress_pct = 0;

    /* Header */
    oled_draw_str(18, 12, "== SLEEP MODE ==");

    /* Sleeping character representation */
    oled_draw_str(36, 28, "(- . -) z Z");

    /* Countdown text */
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "Tidur dalam %d dtk...", remaining_sec);
    oled_draw_str(20, 44, count_str);

    /* Progress bar outline & fill */
    oled_draw_R_frame(14, 52, 100, 8, 2);
    int bar_w = (progress_pct * 96) / 100;
    if (bar_w > 0) {
        oled_draw_box(16, 54, bar_w, 4);
    }

    oled_send_buffer();
}

void power_sleep_enter(void) {
    ESP_LOGI(TAG, "Entering Sleep Mode sequence...");

    /* 1. Show Good Night message */
    oled_clear_buffer();
    oled_set_font(u8g2_font_my_chinese);
    oled_draw_str(16, 16, "== GOOD NIGHT ==");
    oled_draw_str(34, 34, "(u _ u) z Z Z");
    oled_draw_str(12, 52, "Sentuh untuk bangun");
    oled_send_buffer();

    /* 2. Play soft falling sleep chime */
    buzzer_play_sleep_melody();

    vTaskDelay(pdMS_TO_TICKS(400));

    /* 3. Turn OLED off completely to eliminate power draw */
    oled_clear();
    oled_set_power(false);

    /* 4. Wait for user to release their finger from touch sensor */
    ESP_LOGI(TAG, "Waiting for touch release on GPIO %d...", (int)CONFIG_CARMOOD_TOUCH_GPIO);
    while (gpio_get_level((gpio_num_t)CONFIG_CARMOOD_TOUCH_GPIO) == 1) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(250));

    /* 5. Configure GPIO Wakeup on Touch Sensor (GPIO 7) */
    esp_err_t err = gpio_wakeup_enable((gpio_num_t)CONFIG_CARMOOD_TOUCH_GPIO, GPIO_INTR_HIGH_LEVEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_wakeup_enable failed: %s", esp_err_to_name(err));
    }

    err = esp_sleep_enable_gpio_wakeup();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_sleep_enable_gpio_wakeup failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Entering Light Sleep now... (Tap GPIO %d to wake)", (int)CONFIG_CARMOOD_TOUCH_GPIO);

    /* 6. Enter light sleep mode */
    esp_light_sleep_start();

    /* 7. Woken up! Restart cleanly to play boot intro and resume normal operation */
    ESP_LOGI(TAG, "Wakeup triggered by touch! Restarting cleanly...");
    esp_restart();
}
