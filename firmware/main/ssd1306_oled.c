#include <string.h>
#include "ssd1306_oled.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "sdkconfig.h"

static const char *TAG = "oled";

/* 引脚定义（默认值可通过 menuconfig 覆盖） */
#ifndef CONFIG_CARMOOD_OLED_PIN_SCK
#define CONFIG_CARMOOD_OLED_PIN_SCK 13
#endif
#ifndef CONFIG_CARMOOD_OLED_PIN_MOSI
#define CONFIG_CARMOOD_OLED_PIN_MOSI 12
#endif
#ifndef CONFIG_CARMOOD_OLED_PIN_CS
#define CONFIG_CARMOOD_OLED_PIN_CS 9
#endif
#ifndef CONFIG_CARMOOD_OLED_PIN_DC
#define CONFIG_CARMOOD_OLED_PIN_DC 10
#endif
#ifndef CONFIG_CARMOOD_OLED_PIN_RST
#define CONFIG_CARMOOD_OLED_PIN_RST 11
#endif

#define PIN_OLED_SCK   ((gpio_num_t)CONFIG_CARMOOD_OLED_PIN_SCK)
#define PIN_OLED_MOSI  ((gpio_num_t)CONFIG_CARMOOD_OLED_PIN_MOSI)
#define PIN_OLED_CS    ((gpio_num_t)CONFIG_CARMOOD_OLED_PIN_CS)
#define PIN_OLED_DC    ((gpio_num_t)CONFIG_CARMOOD_OLED_PIN_DC)
#define PIN_OLED_RST   ((gpio_num_t)CONFIG_CARMOOD_OLED_PIN_RST)

#define OLED_SPI_HOST  SPI2_HOST
#define OLED_SPI_FREQ  (10 * 1000 * 1000)  // 10 MHz
#define OLED_PAGE_COUNT 8
#define OLED_COLUMN_COUNT 128
/* SH1106 常见需要列偏移 2；若图像整体左右偏移可改成 0/2/4 试验 */
#define OLED_COLUMN_OFFSET 2

/* 中景园 1.3 竖屏资料使用 64 列 x 16 页 = 1024 字节 */
#define OLED_BUF_SIZE  (OLED_WIDTH * OLED_HEIGHT / 8)
static uint8_t s_framebuf[OLED_BUF_SIZE];

static spi_device_handle_t s_spi = NULL;

static esp_err_t oled_write_byte(uint8_t value, bool is_data)
{
    gpio_set_level(PIN_OLED_DC, is_data ? 1 : 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &value,
    };
    return spi_device_polling_transmit(s_spi, &t);
}

static esp_err_t oled_write_cmd(uint8_t cmd)
{
    return oled_write_byte(cmd, false);
}

static esp_err_t oled_write_data(uint8_t data)
{
    return oled_write_byte(data, true);
}

static esp_err_t oled_hw_reset(void)
{
    gpio_set_level(PIN_OLED_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_OLED_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_OLED_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

esp_err_t oled_init(void)
{
    /* 1. 初始化 DC/RST GPIO */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << PIN_OLED_DC) | (1ULL << PIN_OLED_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), TAG, "GPIO 初始化失败");

    /* 2. 初始化 SPI 总线 */
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = PIN_OLED_SCK,
        .mosi_io_num = PIN_OLED_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OLED_BUF_SIZE + 64,
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize(OLED_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO),
        TAG, "SPI 总线初始化失败");

    /* 3. 添加 SPI 设备 */
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = OLED_SPI_FREQ,
        .mode = 0,
        .spics_io_num = PIN_OLED_CS,
        .queue_size = 4,
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_add_device(OLED_SPI_HOST, &dev_cfg, &s_spi),
        TAG, "SPI 设备添加失败");

    /* 4. 硬件复位 */
    ESP_RETURN_ON_ERROR(oled_hw_reset(), TAG, "OLED 复位失败");

    /* 5. 使用 128x64 常用初始化序列（兼容 SSD1306/SH1106） */
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xAE), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xD5), TAG, "init cmd failed");  // 时钟分频
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x80), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xA8), TAG, "init cmd failed");  // 多路复用
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x3F), TAG, "init cmd failed");  // 1/64 duty
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xD3), TAG, "init cmd failed");  // 显示偏移
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x00), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x40), TAG, "init cmd failed");  // 显示起始行
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x8D), TAG, "init cmd failed");  // 电荷泵
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x14), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x20), TAG, "init cmd failed");  // 内存地址模式
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x00), TAG, "init cmd failed");  // 水平寻址
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xA1), TAG, "init cmd failed");  // SEG remap
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xC8), TAG, "init cmd failed");  // COM 扫描方向
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xDA), TAG, "init cmd failed");  // COM 引脚配置
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x12), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x81), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xCF), TAG, "init cmd failed");  // 对比度
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xD9), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xF1), TAG, "init cmd failed");  // 预充电
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xDB), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0x40), TAG, "init cmd failed");  // VCOMH
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xA4), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xA6), TAG, "init cmd failed");
    ESP_RETURN_ON_ERROR(oled_write_cmd(0xAF), TAG, "init cmd failed");

    /* 6. 清空帧缓冲并刷屏 */
    memset(s_framebuf, 0x00, OLED_BUF_SIZE);
    ESP_RETURN_ON_ERROR(oled_flush(), TAG, "初始清屏失败");

    ESP_LOGI(TAG, "OLED 初始化完成（128x64 映射）");
    return ESP_OK;
}

esp_err_t oled_flush(void)
{
    for (uint8_t page = 0; page < OLED_PAGE_COUNT; page++) {
        uint8_t col_addr = OLED_COLUMN_OFFSET;
        ESP_RETURN_ON_ERROR(oled_write_cmd(0xB0 + page), TAG, "set page failed");
        ESP_RETURN_ON_ERROR(oled_write_cmd(0x00 | (col_addr & 0x0F)), TAG, "set low col failed");
        ESP_RETURN_ON_ERROR(oled_write_cmd(0x10 | ((col_addr >> 4) & 0x0F)), TAG, "set high col failed");

        gpio_set_level(PIN_OLED_DC, 1);
        spi_transaction_t t = {
            .length    = OLED_COLUMN_COUNT * 8,
            .tx_buffer = &s_framebuf[page * OLED_COLUMN_COUNT],
        };
        ESP_RETURN_ON_ERROR(spi_device_polling_transmit(s_spi, &t), TAG, "flush page failed");
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
    /* 标准 128x64 页模式：每页 8 行，LSB 为页内低位行 */
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
    memcpy(s_framebuf, data, OLED_BUF_SIZE);
}
