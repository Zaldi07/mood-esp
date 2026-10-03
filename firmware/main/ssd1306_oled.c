#include "ssd1306_oled.h"

#include <string.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "sdkconfig.h"

static const char *TAG = "oled";

#ifndef CONFIG_CARMOOD_OLED_I2C_PORT
#define CONFIG_CARMOOD_OLED_I2C_PORT 0
#endif
#ifndef CONFIG_CARMOOD_OLED_I2C_SDA
#define CONFIG_CARMOOD_OLED_I2C_SDA 8
#endif
#ifndef CONFIG_CARMOOD_OLED_I2C_SCL
#define CONFIG_CARMOOD_OLED_I2C_SCL 9
#endif
#ifndef CONFIG_CARMOOD_OLED_I2C_ADDR
#define CONFIG_CARMOOD_OLED_I2C_ADDR 0x3C
#endif
#ifndef CONFIG_CARMOOD_OLED_COLUMN_OFFSET
#define CONFIG_CARMOOD_OLED_COLUMN_OFFSET 0
#endif

#define OLED_I2C_PORT      ((i2c_port_t)CONFIG_CARMOOD_OLED_I2C_PORT)
#define OLED_I2C_SDA       ((gpio_num_t)CONFIG_CARMOOD_OLED_I2C_SDA)
#define OLED_I2C_SCL       ((gpio_num_t)CONFIG_CARMOOD_OLED_I2C_SCL)
#define OLED_I2C_ADDR      ((uint8_t)CONFIG_CARMOOD_OLED_I2C_ADDR)
#define OLED_COLUMN_OFFSET CONFIG_CARMOOD_OLED_COLUMN_OFFSET

#define OLED_PAGE_COUNT    8
#define OLED_COLUMN_COUNT  128
#define OLED_BUF_SIZE      (OLED_WIDTH * OLED_HEIGHT / 8)

static uint8_t s_framebuf[OLED_BUF_SIZE];
static uint8_t s_oled_addr = OLED_I2C_ADDR;

static esp_err_t oled_write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_write_to_device(OLED_I2C_PORT, s_oled_addr, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

esp_err_t oled_init(void)
{
    /* 1. 配置 I2C 参数 */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_I2C_SDA,
        .scl_io_num = OLED_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    esp_err_t ret = i2c_param_config(OLED_I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(OLED_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. 探测 OLED I2C 地址 (0x3C 或 0x3D) */
    const uint8_t probe_addrs[] = {OLED_I2C_ADDR, (uint8_t)(OLED_I2C_ADDR == 0x3C ? 0x3D : 0x3C)};
    bool found = false;
    for (int i = 0; i < 2; i++) {
        uint8_t dummy_cmd[2] = {0x00, 0xE3}; // NOP command
        if (i2c_master_write_to_device(OLED_I2C_PORT, probe_addrs[i], dummy_cmd, sizeof(dummy_cmd), pdMS_TO_TICKS(50)) == ESP_OK) {
            s_oled_addr = probe_addrs[i];
            found = true;
            ESP_LOGI(TAG, "检测到 OLED I2C 地址: 0x%02X", s_oled_addr);
            break;
        }
    }
    if (!found) {
        ESP_LOGW(TAG, "未收到 OLED 探测响应，默认尝试使用 0x%02X", s_oled_addr);
    }

    /* 3. SSD1306 初始化指令序列 */
    static const uint8_t init_cmds[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Set Display Clock Divide Ratio/Oscillator Frequency
        0xA8, 0x3F, // Set Multiplex Ratio (1/64)
        0xD3, 0x00, // Set Display Offset (0)
        0x40,       // Set Display Start Line (0)
        0x8D, 0x14, // Charge Pump Setting (Enable)
        0x20, 0x02, // Set Memory Addressing Mode (Page Addressing Mode)
        0xA1,       // Set Segment Re-map (0xA1 = column address 127 is mapped to SEG0)
        0xC8,       // Set COM Output Scan Direction (0xC8 = remapped mode)
        0xDA, 0x12, // Set COM Pins Hardware Configuration
        0x81, 0xCF, // Set Contrast Control
        0xD9, 0xF1, // Set Pre-charge Period
        0xDB, 0x40, // Set VCOMH Deselect Level
        0xA4,       // Entire Display ON (Resume from RAM)
        0xA6,       // Set Normal Display
        0xAF        // Display ON
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        ESP_RETURN_ON_ERROR(oled_write_cmd(init_cmds[i]), TAG, "Init cmd 0x%02X failed", init_cmds[i]);
    }

    /* 4. 清空缓冲并刷屏 */
    memset(s_framebuf, 0x00, OLED_BUF_SIZE);
    ESP_RETURN_ON_ERROR(oled_flush(), TAG, "初始清屏失败");

    ESP_LOGI(TAG, "OLED (I2C) 初始化完成 (128x64)");
    return ESP_OK;
}

esp_err_t oled_flush(void)
{
    uint8_t cmd_buf[4];
    cmd_buf[0] = 0x00; // Control byte: Command

    uint8_t page_buf[1 + OLED_COLUMN_COUNT];
    page_buf[0] = 0x40; // Control byte: Data

    for (uint8_t page = 0; page < OLED_PAGE_COUNT; page++) {
        uint8_t col_addr = OLED_COLUMN_OFFSET;
        cmd_buf[1] = 0xB0 + page;
        cmd_buf[2] = 0x00 | (col_addr & 0x0F);
        cmd_buf[3] = 0x10 | ((col_addr >> 4) & 0x0F);

        esp_err_t err = i2c_master_write_to_device(
            OLED_I2C_PORT, s_oled_addr, cmd_buf, sizeof(cmd_buf), pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            return err;
        }

        memcpy(&page_buf[1], &s_framebuf[page * OLED_COLUMN_COUNT], OLED_COLUMN_COUNT);
        err = i2c_master_write_to_device(
            OLED_I2C_PORT, s_oled_addr, page_buf, sizeof(page_buf), pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

void oled_clear_buf(void)
{
    memset(s_framebuf, 0x00, OLED_BUF_SIZE);
}

esp_err_t oled_clear(void)
{
    oled_clear_buf();
    return oled_flush();
}

esp_err_t oled_fill(void)
{
    memset(s_framebuf, 0xFF, OLED_BUF_SIZE);
    return oled_flush();
}

void oled_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }
    uint16_t idx = (uint16_t)((y / 8) * OLED_COLUMN_COUNT + x);
    uint8_t bit = (uint8_t)(1U << (y % 8));
    if (on) {
        s_framebuf[idx] |= bit;
    } else {
        s_framebuf[idx] &= ~bit;
    }
}

void oled_draw_bitmap(const uint8_t *data)
{
    if (data != NULL) {
        memcpy(s_framebuf, data, OLED_BUF_SIZE);
    }
}

esp_err_t oled_set_power(bool on)
{
    return oled_write_cmd(on ? 0xAF : 0xAE);
}

