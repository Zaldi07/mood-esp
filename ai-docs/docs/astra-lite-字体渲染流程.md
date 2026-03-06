---
name: Astra Lite 字体渲染流程
description: 记录 Astra Lite UI 框架中文字体从构建生成到运行时渲染的完整链路。
created_at: 2026-03-06
---

# Astra Lite 字体渲染流程

## 概述

Astra Lite 的文字渲染分为 **构建时（字体生成）** 和 **运行时（屏幕绘制）** 两个阶段。
构建时从源码中按需提取中文字符，生成 u8g2 字体子集；运行时通过 u8g2 库在 SSD1306 OLED 上绘制。

## 构建时：字体生成管线

### 核心脚本

`scripts/generate_astra_lite_font.py`

### 流程

1. **扫描源码提取中文字符**
   - `--source` 指向 `firmware/main/ui/astra_menu_bridge.c`
   - `--include-regex 'astra_new_button_item\('` 只匹配菜单项构造行
   - 从字符串字面量中提取 CJK 统一汉字（U+4E00–U+9FFF），去重并保持出现顺序

2. **输出中间文件**（存放于 `firmware/main/ui/astra_lite/Others/`）
   - `chars.txt`：去重有序的中文字符列表（如 `木鱼模式方向标定`）
   - `str.map`：bdfconv 映射文件，格式 `32-128,$6728,$9c7c,...`

3. **生成字体 C 文件**（可选，需额外参数）
   - 同时提供 `--bdfconv`、`--bdf`（WQY 文泉驿 12pt）、`--font-out` 时
   - 调用 bdfconv 从 BDF 字体中导出只含所需字符的子集
   - 输出 `u8g2_font_my_chinese.c`（约 9KB）

### 命令示例

仅更新 chars.txt 和 str.map：

```bash
uv run python3 scripts/generate_astra_lite_font.py \
  --source firmware/main/ui/astra_menu_bridge.c \
  --include-regex 'astra_new_button_item\(' \
  --chars-out firmware/main/ui/astra_lite/Others/chars.txt \
  --map-out firmware/main/ui/astra_lite/Others/str.map
```

连同字体 C 文件一起生成：

```bash
uv run python3 scripts/generate_astra_lite_font.py \
  --source firmware/main/ui/astra_menu_bridge.c \
  --include-regex 'astra_new_button_item\(' \
  --chars-out firmware/main/ui/astra_lite/Others/chars.txt \
  --map-out firmware/main/ui/astra_lite/Others/str.map \
  --bdfconv /path/to/bdfconv \
  --bdf /path/to/wenquanyi_12pt.bdf \
  --font-out firmware/main/ui/astra_lite/Others/u8g2_font_my_chinese.c
```

## 运行时：渲染链路

### 模块关系

```
astra_menu_bridge.c  →  astra_ui_core.c  →  astra_ui_drawer.c
        ↓                                          ↓
  astra_ui_item.c                        astra_ui_draw_driver.c/h
        ↓                                          ↓
   u8g2_font_my_chinese[]  ←──────────  u8g2 库 + SSD1306 驱动
```

### 关键步骤

1. **驱动初始化**（`astra_ui_draw_driver.c`）
   - `astra_ui_driver_init()` 设置 u8g2 为 SSD1306 128×64 全缓冲模式
   - 默认加载 `u8g2_font_my_chinese` 字体，开启透明字体模式

2. **字体切换缓存**（`astra_ui_item.c`）
   - `astra_set_font()` 维护 `astra_font_current` 指针，只有字体变化时才调用 `u8g2_SetFont()`

3. **文字绘制**（`astra_ui_drawer.c`）
   - 列表项：`oled_draw_UTF8(x, y, content)` → `u8g2_DrawUTF8()`
   - 宽度计算：`oled_get_UTF8_width()` / `oled_get_str_width()`
   - 高度计算：`oled_get_str_height()` = Ascent − Descent
   - 信息栏/弹窗同样走 `oled_draw_UTF8` 渲染

4. **帧输出**（`astra_menu_bridge.c`）
   - `astra_menu_tick()` 每帧执行：清屏 → 渲染主体 + 控件 → 发送缓冲
   - `oled_send_buffer()` 将 u8g2 帧缓冲写入 SSD1306

### Draw Driver 宏抽象层

`astra_ui_draw_driver.h` 将所有绘图操作封装为 `oled_*` 宏，底层调用 u8g2 API：

| 宏 | 用途 |
|---|---|
| `oled_set_font(font)` | 设置当前字体 |
| `oled_draw_UTF8(x,y,str)` | 绘制 UTF-8 字符串 |
| `oled_get_UTF8_width(str)` | 获取 UTF-8 字符串像素宽度 |
| `oled_get_str_height()` | 获取当前字体行高 |
| `oled_draw_str(x,y,str)` | 绘制 ASCII 字符串 |
| `oled_clear_buffer()` | 清空帧缓冲 |
| `oled_send_buffer()` | 将帧缓冲刷新到 OLED |

## 扩展指南

- **新增菜单项**：在 `astra_menu_bridge.c` 添加 `astra_new_button_item(...)` 后重跑字体生成脚本即可
- **扫描更多文件**：追加 `--source` 参数，按需调整 `--include-regex`
- **换字体/字号**：替换 `--bdf` 参数指向的 BDF 文件
- **换显示屏**：只需修改 `astra_ui_draw_driver.c/h`，上层代码无需变动
