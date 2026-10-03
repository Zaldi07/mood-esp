#include "ui/boot_intro.h"
#include "ui/boot_intro_data.h"
#include "ssd1306_oled.h"
#include "drivers/touch_input.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "boot_intro";
static uint8_t s_intro_frame_buf[BOOT_INTRO_FRAME_BUF_SIZE];

static void rle_decode_to_buf(uint8_t *dst_buf, const uint8_t *src, uint16_t src_len) {
    int dst = 0;
    int i = 0;
    const int buf_size = BOOT_INTRO_FRAME_BUF_SIZE;

    while (i < src_len && dst < buf_size) {
        uint8_t header = src[i++];
        if (header & 0x80) {
            int count = (header & 0x7F) + 1;
            if (i >= src_len) break;
            uint8_t val = src[i++];
            for (int j = 0; j < count && dst < buf_size; j++) {
                dst_buf[dst++] = val;
            }
        } else {
            int count = header + 1;
            for (int j = 0; j < count && i < src_len && dst < buf_size; j++) {
                dst_buf[dst++] = src[i++];
            }
        }
    }
    while (dst < buf_size) {
        dst_buf[dst++] = 0;
    }
}

void boot_intro_run(void) {
    ESP_LOGI(TAG, "Starting boot intro animation (%d frames)", BOOT_INTRO_FRAME_COUNT);

    for (int i = 0; i < BOOT_INTRO_FRAME_COUNT; i++) {
        touch_input_event_t touch_event = {};
        if (touch_input_poll(&touch_event) == ESP_OK) {
            touch_input_key_event_t top_key = touch_event.keys[TOUCH_KEY_TOP];
            if (top_key.valid_click || touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
                ESP_LOGI(TAG, "Boot intro skipped by touch input");
                break;
            }
        }

        rle_decode_to_buf(s_intro_frame_buf, s_boot_intro_frames[i], s_boot_intro_sizes[i]);
        oled_draw_bitmap(s_intro_frame_buf);
        oled_flush();

        /* At 400kHz I2C, oled_flush takes ~23ms. Delay 10ms yields ~33ms per frame (~30 FPS),
         * matching the ~3.25s duration of the space chiptune intro melody on GPIO 5. */
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    oled_clear_buf();
    oled_flush();
    ESP_LOGI(TAG, "Boot intro animation finished");
}
