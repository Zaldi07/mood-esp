---
name: ESP32-S3 SuperMini 完整接线与配置
description: 记录 CarMood 项目所有外设接线、方向映射、Wi-Fi/NTP 配置。
updated_at: 2026-03-02
---

# CarMood 接线与配置总览

## 引脚总表

| 功能 | 外设引脚 | ESP32-S3 引脚 | 协议 | 说明 |
| --- | --- | --- | --- | --- |
| OLED CLK | CLK / SCL / SCK | GPIO13 | SPI | 时钟 |
| OLED MOSI | MOSI / SDA / DIN | GPIO12 | SPI | 数据 |
| OLED RST | RES / RST | GPIO11 | SPI | 复位 |
| OLED DC | DC | GPIO10 | SPI | 数据/命令选择 |
| OLED CS | CS | GPIO9 | SPI | 片选 |
| 触摸（顶部） | 铜箔电极 | GPIO5 | Touch CH5 | 串联 1kΩ，表情切换 + 长按标定 |
| 触摸（右上） | 铜箔电极 | GPIO7 | Touch CH7 | 串联 1kΩ，功能待定 |
| 触摸（右下） | 铜箔电极 | GPIO6 | Touch CH6 | 串联 1kΩ，功能待定 |
| LIS3DH VCC | VCC | 3V3 | — | 传感器供电 |
| LIS3DH GND | GND | GND | — | 共地 |
| LIS3DH SDA | SDA | GPIO38 | I2C | 数据线 |
| LIS3DH SCL | SCL | GPIO37 | I2C | 时钟线，400kHz |
| LIS3DH INT1 | INT1 | GPIO36 | — | 中断输出（预留） |

## 1. OLED 显示屏（SPI，SH1106 1.3 寸单色）

| OLED 引脚 | ESP32-S3 引脚 |
| --- | --- |
| CLK / SCL / SCK | GPIO13 |
| MOSI / SDA / DIN | GPIO12 |
| RES / RST | GPIO11 |
| DC | GPIO10 |
| CS | GPIO9 |

- SPI 频率：10 MHz
- 分辨率：128×64，单色
- 帧缓冲：1024 字节（页模式），DMA 批量刷新

## 2. 铜箔触摸传感器（3 通道）

```
        ┌─────────────────────┐
        │    [GPIO5 顶部]      │  ← 表情切换 + 长按标定
        │                     │
        │                     │  [GPIO7 右上] ← 功能待定
        │      OLED 屏幕      │
        │                     │  [GPIO6 右下] ← 功能待定
        │                     │
        └─────────────────────┘
```

| 位置 | ESP32-S3 引脚 | Touch 通道 | 当前功能 |
| --- | --- | --- | --- |
| 顶部 | GPIO5 | CH5 | 单击切换表情，长按 ≥ 1.2s 触发方向标定 |
| 右上 | GPIO7 | CH7 | 功能待定 |
| 右下 | GPIO6 | CH6 | 待定（已接通，有日志输出） |

- 每个铜箔电极串联 1kΩ 电阻抗干扰
- 按下阈值：delta ≥ 300
- 释放阈值：delta ≤ 120
- 去抖：连续 3 次采样确认
- 有效点击：40ms ~ 800ms

## 3. LIS3DH 三轴加速度计（I2C）

| LIS3DH 引脚 | ESP32-S3 引脚 | 说明 |
| --- | --- | --- |
| VCC | 3V3 | 传感器供电 |
| GND | GND | 共地 |
| SDA | GPIO38 | I2C 数据线 |
| SCL | GPIO37 | I2C 时钟线 |
| INT1 | GPIO36 | 中断输出（预留） |

- I2C 地址：0x18
- I2C 频率：400 kHz
- I2C 端口：I2C_NUM_0

### 方向语义映射

| 方向 | 判定条件 | 触发表情 |
| --- | --- | --- |
| 左 | delta.x < -300 | EXPR_TURN_LEFT |
| 右 | delta.x > 300 | EXPR_TURN_RIGHT |
| 前（前倾） | delta.y < -300 | — |
| 后（后仰） | delta.y > 300 | — |
| 回中 | \|delta.x\| ≤ 300 且 \|delta.y\| ≤ 300 | 恢复当前选中表情 |

- 该映射基于当前安装姿态，更换摆放方向需重新标定
- 长按触摸 ≥ 1200ms 进入引导标定流程

## 4. Wi-Fi / NTP 校时

| 配置项 | 默认值 | 说明 |
| --- | --- | --- |
| Wi-Fi SSID | iQOO 13 | 联网校时用 |
| Wi-Fi Password | 88888888 | — |
| NTP Server | ntp.aliyun.com | 阿里云 NTP |
| 时区 | CST-8 | 中国标准时间 |
| 超时 | 30 秒 | Wi-Fi 连接 + NTP 等待 |
| 开机自动校时 | 是 | 启动时自动连接 Wi-Fi 校时 |

## 5. 供电

- 电池：3.7V 锂电池 → TP4056 充放电管理
- 系统供电：TP4056 输出 3.3V → ESP32-S3、OLED、LIS3DH、PAM8302
- 开关：SS12D07VG4（SPDT 滑动开关），COM 接 TP4056 输出，ON 接系统 3.3V

## 备注

- 所有引脚可通过 `menuconfig`（CarMood Board Config）修改
- 配置文件：`firmware/sdkconfig.defaults`
- 若调整引脚，请同步更新本文档与 sdkconfig.defaults
