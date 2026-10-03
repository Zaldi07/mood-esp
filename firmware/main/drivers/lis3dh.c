#include "drivers/lis3dh.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "lis3dh";

#ifndef CONFIG_CARMOOD_LIS3DH_I2C_PORT
#define CONFIG_CARMOOD_LIS3DH_I2C_PORT 0
#endif
#ifndef CONFIG_CARMOOD_LIS3DH_I2C_SDA
#define CONFIG_CARMOOD_LIS3DH_I2C_SDA 8
#endif
#ifndef CONFIG_CARMOOD_LIS3DH_I2C_SCL
#define CONFIG_CARMOOD_LIS3DH_I2C_SCL 9
#endif
#ifndef CONFIG_CARMOOD_LIS3DH_I2C_FREQ_HZ
#define CONFIG_CARMOOD_LIS3DH_I2C_FREQ_HZ 400000
#endif

#define LIS3DH_I2C_PORT ((i2c_port_t)CONFIG_CARMOOD_LIS3DH_I2C_PORT)
#define LIS3DH_I2C_SDA ((gpio_num_t)CONFIG_CARMOOD_LIS3DH_I2C_SDA)
#define LIS3DH_I2C_SCL ((gpio_num_t)CONFIG_CARMOOD_LIS3DH_I2C_SCL)
#define LIS3DH_I2C_FREQ_HZ CONFIG_CARMOOD_LIS3DH_I2C_FREQ_HZ

#define LIS3DH_REG_WHO_AM_I 0x0F
#define LIS3DH_REG_CTRL1 0x20
#define LIS3DH_REG_CTRL4 0x23
#define LIS3DH_REG_OUT_X_L 0x28

#ifndef CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS
#define CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS 3500
#endif
#ifndef CONFIG_CARMOOD_LIS3DH_CALIB_GAP_MS
#define CONFIG_CARMOOD_LIS3DH_CALIB_GAP_MS 800
#endif

typedef enum {
  AXIS_X = 0,
  AXIS_Y = 1,
  AXIS_Z = 2,
} axis_id_t;

static uint8_t s_lis3dh_addr = 0x18;
static int32_t s_ax0 = 0;
static int32_t s_ay0 = 0;
static int32_t s_az0 = 0;
static bool s_lis3dh_zero_ready = false;

/* 方向映射参数（可由自动标定覆盖） */
static axis_id_t s_lr_axis = AXIS_X;
static int s_lr_sign = 1; /* +1: delta>0 为右；-1: delta>0 为左 */
static int32_t s_lr_threshold = 300;
static axis_id_t s_fb_axis = AXIS_Y;
static int s_fb_sign = 1; /* +1: delta>0 为后；-1: delta>0 为前 */
static int32_t s_fb_threshold = 300;

static esp_err_t lis3dh_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t value) {
  uint8_t data[2] = {reg, value};
  return i2c_master_write_to_device(LIS3DH_I2C_PORT, dev_addr, data, sizeof(data),
                                    pdMS_TO_TICKS(100));
}

static esp_err_t lis3dh_read_reg(uint8_t dev_addr, uint8_t reg, uint8_t *value) {
  return i2c_master_write_read_device(LIS3DH_I2C_PORT, dev_addr, &reg, 1, value, 1,
                                      pdMS_TO_TICKS(100));
}

esp_err_t lis3dh_read_raw(int16_t *x, int16_t *y, int16_t *z) {
  uint8_t reg = LIS3DH_REG_OUT_X_L | 0x80; /* 自动地址递增 */
  uint8_t buf[6] = {0};
  ESP_RETURN_ON_ERROR(i2c_master_write_read_device(LIS3DH_I2C_PORT, s_lis3dh_addr,
                                                    &reg, 1, buf, sizeof(buf),
                                                    pdMS_TO_TICKS(100)),
                      TAG, "读取 LIS3DH XYZ 失败");

  int16_t rx = (int16_t)((buf[1] << 8) | buf[0]);
  int16_t ry = (int16_t)((buf[3] << 8) | buf[2]);
  int16_t rz = (int16_t)((buf[5] << 8) | buf[4]);
  /* CTRL4 使用高分辨率模式时，低 4 位无效 */
  *x = rx >> 4;
  *y = ry >> 4;
  *z = rz >> 4;
  return ESP_OK;
}

esp_err_t lis3dh_init(void) {
  i2c_config_t i2c_cfg = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = LIS3DH_I2C_SDA,
      .scl_io_num = LIS3DH_I2C_SCL,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = LIS3DH_I2C_FREQ_HZ,
  };
  i2c_param_config(LIS3DH_I2C_PORT, &i2c_cfg);
  esp_err_t drv_err = i2c_driver_install(LIS3DH_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
  if (drv_err != ESP_OK && drv_err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "I2C 驱动安装失败: %s", esp_err_to_name(drv_err));
    return drv_err;
  }

  /* 依次尝试 0x18/0x19 */
  const uint8_t candidates[2] = {0x18, 0x19};
  uint8_t whoami = 0;
  esp_err_t ret = ESP_FAIL;
  for (int i = 0; i < 2; i++) {
    ret = lis3dh_read_reg(candidates[i], LIS3DH_REG_WHO_AM_I, &whoami);
    if (ret == ESP_OK && whoami == 0x33) {
      s_lis3dh_addr = candidates[i];
      break;
    }
  }
  ESP_RETURN_ON_FALSE(whoami == 0x33, ESP_ERR_NOT_FOUND, TAG,
                      "未检测到 LIS3DH，WHO_AM_I=0x%02X", whoami);

  /* 100Hz，正常模式，XYZ 全开 */
  ESP_RETURN_ON_ERROR(lis3dh_write_reg(s_lis3dh_addr, LIS3DH_REG_CTRL1, 0x57), TAG,
                      "写 CTRL1 失败");
  /* BDU=1, HR=1, 量程 ±2g */
  ESP_RETURN_ON_ERROR(lis3dh_write_reg(s_lis3dh_addr, LIS3DH_REG_CTRL4, 0x88), TAG,
                      "写 CTRL4 失败");

  vTaskDelay(pdMS_TO_TICKS(5));
  ESP_LOGI(TAG, "LIS3DH 初始化完成，I2C 地址=0x%02X, WHO_AM_I=0x%02X", s_lis3dh_addr,
           whoami);
  return ESP_OK;
}

esp_err_t lis3dh_rezero(void) {
  int32_t sum_x = 0;
  int32_t sum_y = 0;
  int32_t sum_z = 0;
  const int sample_count = 40; /* 约 400ms */

  for (int i = 0; i < sample_count; i++) {
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    ESP_RETURN_ON_ERROR(lis3dh_read_raw(&ax, &ay, &az), TAG, "LIS3DH 归零采样失败");
    sum_x += ax;
    sum_y += ay;
    sum_z += az;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  s_ax0 = sum_x / sample_count;
  s_ay0 = sum_y / sample_count;
  s_az0 = sum_z / sample_count;
  s_lis3dh_zero_ready = true;
  ESP_LOGI(TAG, "LIS3DH 基准已重置: x0=%ld y0=%ld z0=%ld", (long)s_ax0, (long)s_ay0,
           (long)s_az0);
  return ESP_OK;
}

static inline int32_t axis_get(axis_id_t axis, int32_t x, int32_t y, int32_t z) {
  if (axis == AXIS_X) {
    return x;
  }
  if (axis == AXIS_Y) {
    return y;
  }
  return z;
}

static const char *axis_name(axis_id_t axis) {
  if (axis == AXIS_X) {
    return "X";
  }
  if (axis == AXIS_Y) {
    return "Y";
  }
  return "Z";
}

static esp_err_t lis3dh_collect_mean(const char *step_name, int sample_ms,
                                     int total_ms, int32_t *mx, int32_t *my,
                                     int32_t *mz) {
  int samples = total_ms / sample_ms;
  if (samples < 5) {
    samples = 5;
  }

  int64_t sx = 0;
  int64_t sy = 0;
  int64_t sz = 0;
  ESP_LOGI(TAG, "校准步骤: %s（请保持动作）", step_name);

  for (int i = 0; i < samples; i++) {
    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;
    ESP_RETURN_ON_ERROR(lis3dh_read_raw(&ax, &ay, &az), TAG, "校准采样失败");
    sx += ax;
    sy += ay;
    sz += az;
    vTaskDelay(pdMS_TO_TICKS(sample_ms));
  }

  *mx = (int32_t)(sx / samples);
  *my = (int32_t)(sy / samples);
  *mz = (int32_t)(sz / samples);
  ESP_LOGI(TAG, "%s 平均值: (%ld,%ld,%ld)", step_name, (long)*mx, (long)*my,
           (long)*mz);
  return ESP_OK;
}

esp_err_t lis3dh_run_guided_calibration(lis3dh_calib_prompt_fn_t prompt_fn) {
  ESP_LOGI(TAG, "=== 开始方向自动标定 ===");
  ESP_LOGI(TAG, "流程: 静止 -> 左倾 -> 右倾 -> 前倾 -> 后仰");
  ESP_LOGI(TAG, "每个动作保持约 %d ms", CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS);

  if (prompt_fn != NULL) {
    prompt_fn("CAL", "START");
  }

  int32_t n_x = 0;
  int32_t n_y = 0;
  int32_t n_z = 0;
  int32_t l_x = 0;
  int32_t l_y = 0;
  int32_t l_z = 0;
  int32_t r_x = 0;
  int32_t r_y = 0;
  int32_t r_z = 0;
  int32_t f_x = 0;
  int32_t f_y = 0;
  int32_t f_z = 0;
  int32_t b_x = 0;
  int32_t b_y = 0;
  int32_t b_z = 0;

  if (prompt_fn != NULL) {
    prompt_fn("STEP 1", "STILL");
  }
  ESP_RETURN_ON_ERROR(lis3dh_collect_mean("静止", 20,
                                          CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS,
                                          &n_x, &n_y, &n_z),
                      TAG, "静止采样失败");

  ESP_LOGI(TAG, "准备左倾...");
  vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_LIS3DH_CALIB_GAP_MS));
  if (prompt_fn != NULL) {
    prompt_fn("STEP 2", "LEFT");
  }
  ESP_RETURN_ON_ERROR(lis3dh_collect_mean("左倾", 20,
                                          CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS,
                                          &l_x, &l_y, &l_z),
                      TAG, "左倾采样失败");

  ESP_LOGI(TAG, "准备右倾...");
  vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_LIS3DH_CALIB_GAP_MS));
  if (prompt_fn != NULL) {
    prompt_fn("STEP 3", "RIGHT");
  }
  ESP_RETURN_ON_ERROR(lis3dh_collect_mean("右倾", 20,
                                          CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS,
                                          &r_x, &r_y, &r_z),
                      TAG, "右倾采样失败");

  ESP_LOGI(TAG, "准备前倾...");
  vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_LIS3DH_CALIB_GAP_MS));
  if (prompt_fn != NULL) {
    prompt_fn("STEP 4", "FORWARD");
  }
  ESP_RETURN_ON_ERROR(lis3dh_collect_mean("前倾", 20,
                                          CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS,
                                          &f_x, &f_y, &f_z),
                      TAG, "前倾采样失败");

  ESP_LOGI(TAG, "准备后仰...");
  vTaskDelay(pdMS_TO_TICKS(CONFIG_CARMOOD_LIS3DH_CALIB_GAP_MS));
  if (prompt_fn != NULL) {
    prompt_fn("STEP 5", "BACK");
  }
  ESP_RETURN_ON_ERROR(lis3dh_collect_mean("后仰", 20,
                                          CONFIG_CARMOOD_LIS3DH_CALIB_STEP_MS,
                                          &b_x, &b_y, &b_z),
                      TAG, "后仰采样失败");

  s_ax0 = n_x;
  s_ay0 = n_y;
  s_az0 = n_z;
  s_lis3dh_zero_ready = true;

  int32_t dl[3] = {l_x - n_x, l_y - n_y, l_z - n_z};
  int32_t dr[3] = {r_x - n_x, r_y - n_y, r_z - n_z};
  int32_t df[3] = {f_x - n_x, f_y - n_y, f_z - n_z};
  int32_t db[3] = {b_x - n_x, b_y - n_y, b_z - n_z};

  int32_t lr_best = -1;
  axis_id_t lr_axis = AXIS_X;
  for (int i = 0; i < 3; i++) {
    int32_t score = llabs((long long)dl[i]) + llabs((long long)dr[i]);
    if (score > lr_best) {
      lr_best = score;
      lr_axis = (axis_id_t)i;
    }
  }
  s_lr_axis = lr_axis;
  int32_t dr_axis = axis_get(lr_axis, dr[0], dr[1], dr[2]);
  s_lr_sign = (dr_axis >= 0) ? 1 : -1;
  int32_t dl_axis = axis_get(lr_axis, dl[0], dl[1], dl[2]);
  int32_t lr_min =
      (int32_t)((llabs((long long)dl_axis) < llabs((long long)dr_axis))
                    ? llabs((long long)dl_axis)
                    : llabs((long long)dr_axis));
  s_lr_threshold = lr_min / 2;
  if (s_lr_threshold < 200) {
    s_lr_threshold = 200;
  }

  int32_t fb_best = -1;
  axis_id_t fb_axis = AXIS_Y;
  for (int i = 0; i < 3; i++) {
    int32_t score = llabs((long long)df[i]) + llabs((long long)db[i]);
    if (score > fb_best) {
      fb_best = score;
      fb_axis = (axis_id_t)i;
    }
  }
  s_fb_axis = fb_axis;
  int32_t db_axis = axis_get(fb_axis, db[0], db[1], db[2]);
  s_fb_sign = (db_axis >= 0) ? 1 : -1;
  int32_t df_axis = axis_get(fb_axis, df[0], df[1], df[2]);
  int32_t fb_min =
      (int32_t)((llabs((long long)df_axis) < llabs((long long)db_axis))
                    ? llabs((long long)df_axis)
                    : llabs((long long)db_axis));
  s_fb_threshold = fb_min / 2;
  if (s_fb_threshold < 200) {
    s_fb_threshold = 200;
  }

  ESP_LOGI(TAG, "=== 标定完成 ===");
  ESP_LOGI(TAG, "基准: x0=%ld y0=%ld z0=%ld", (long)s_ax0, (long)s_ay0,
           (long)s_az0);
  ESP_LOGI(TAG, "左右轴: %s, sign=%d, threshold=%ld", axis_name(s_lr_axis),
           s_lr_sign, (long)s_lr_threshold);
  ESP_LOGI(TAG, "前后轴: %s, sign=%d, threshold=%ld", axis_name(s_fb_axis),
           s_fb_sign, (long)s_fb_threshold);
  ESP_LOGI(TAG, "语义: sign=+1 表示右/后为正；sign=-1 表示左/前为正");

  if (prompt_fn != NULL) {
    prompt_fn("CAL", "DONE");
  }
  return ESP_OK;
}

bool lis3dh_is_zero_ready(void) { return s_lis3dh_zero_ready; }

void lis3dh_get_delta(int16_t ax, int16_t ay, int16_t az, int32_t *dx, int32_t *dy,
                      int32_t *dz) {
  if (dx != NULL) {
    *dx = (int32_t)ax - s_ax0;
  }
  if (dy != NULL) {
    *dy = (int32_t)ay - s_ay0;
  }
  if (dz != NULL) {
    *dz = (int32_t)az - s_az0;
  }
}

void lis3dh_eval_direction(int32_t dx, int32_t dy, int32_t dz, char *turn_state_char,
                           char *pitch_state_char, int32_t *lr_val,
                           int32_t *fb_val) {
  int32_t local_lr = axis_get(s_lr_axis, dx, dy, dz) * s_lr_sign;
  int32_t local_fb = axis_get(s_fb_axis, dx, dy, dz) * s_fb_sign;

  char turn = 'C';
  char pitch = 'N';
  if (local_lr > s_lr_threshold) {
    turn = 'R';
  } else if (local_lr < -s_lr_threshold) {
    turn = 'L';
  }

  if (local_fb > s_fb_threshold) {
    pitch = 'B';
  } else if (local_fb < -s_fb_threshold) {
    pitch = 'F';
  }

  if (turn_state_char != NULL) {
    *turn_state_char = turn;
  }
  if (pitch_state_char != NULL) {
    *pitch_state_char = pitch;
  }
  if (lr_val != NULL) {
    *lr_val = local_lr;
  }
  if (fb_val != NULL) {
    *fb_val = local_fb;
  }
}
