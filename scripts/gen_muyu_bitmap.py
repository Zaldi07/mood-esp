#!/usr/bin/env python3
"""
根据参考图生成 48x48 木鱼位图数组。
参考图：木鱼侧面视图，圆润桃形主体，左上方小把手，中间一条优雅弧线裂缝。
"""

from PIL import Image, ImageDraw
import math

W, H = 48, 48
img = Image.new("1", (W, H), 0)
draw = ImageDraw.Draw(img)

# 主体：一个大椭圆，木鱼身体饱满圆润
draw.ellipse([4, 10, 44, 47], fill=1)
draw.ellipse([6, 7, 42, 44], fill=1)

# 把手 - 左上方小突起
draw.ellipse([10, 1, 28, 13], fill=1)
draw.ellipse([7, 0, 20, 9], fill=1)

# 鱼嘴裂缝 - 更精细的弧线
# 使用抗锯齿风格的子像素绘制
for t in range(400):
    frac = t / 399.0
    x = 42 - frac * 36
    y_center = 28 + 7 * math.sin(frac * math.pi * 0.85)
    # 裂缝宽度：两端窄(1px)中间宽(3px)
    half_w = 0.3 + 1.5 * math.sin(frac * math.pi)
    
    for sub_y in range(int(y_center - half_w - 1), int(y_center + half_w + 2)):
        if abs(sub_y - y_center) <= half_w:
            px = int(round(x))
            if 0 <= px < W and 0 <= sub_y < H:
                img.putpixel((px, sub_y), 0)

# 输出 C 数组
lines = []
lines.append("static const uint8_t s_muyu_48x48[288] = {")
for row in range(H):
    line = "    "
    for byte_idx in range(6):
        val = 0
        for bit in range(8):
            col = byte_idx * 8 + bit
            if col < W and img.getpixel((col, row)):
                val |= (1 << (7 - bit))
        line += f"0x{val:02X}, "
    lines.append(line)
lines.append("};")
print("\n".join(lines))

img_preview = img.resize((W * 4, H * 4), Image.NEAREST)
img_preview.save("/Users/jzh/Projects/carmood/scripts/muyu_preview.png")
print("\n// 预览已保存到 scripts/muyu_preview.png")
