#!/usr/bin/env python3
"""
生成机器人脸表情帧序列与 GIF，同时输出 C 帧数据头文件 face_anim_data.h。
Python 是唯一的绘图源，C 端只做播放。
所有可调参数都在本文件顶部，改这里就行。
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

# ---------------------------------------------------------------------------
# 可调参数（改这里 → 跑脚本 → GIF 预览 + C 帧数据同步更新）
# ---------------------------------------------------------------------------

# 布局常量
EYE_L_CX   = 38    # 左眼中心 x
EYE_R_CX   = 90    # 右眼中心 x
EYE_CY     = 32    # 眼睛中心 y
EYE_HW     = 18    # 眼框半宽
EYE_HH     = 20    # 眼框半高
MOUTH_Y    = 54    # 嘴巴 y
EYE_RADIUS = 8     # 眼框圆角
PUPIL_R    = 5     # 瞳孔半径

# 动画周期（ms）
CYCLE_MS = 1200

# idle
IDLE_DY_AMP       = 2
IDLE_PUPIL_DY_AMP = 1

# blink
BLINK_CLOSE_START = 0.40
BLINK_CLOSE_END   = 0.50

# happy
HAPPY_DY_AMP      = 5
HAPPY_SMILE_BASE  = 3
HAPPY_SMILE_AMP   = 3

# sleepy
SLEEPY_SCALE_BASE  = 0.35
SLEEPY_SCALE_AMP   = 0.15
SLEEPY_DY_AMP      = 6
SLEEPY_PUPIL_R     = 4
SLEEPY_CLOSE_START = 0.40
SLEEPY_CLOSE_END   = 0.55

# turn
TURN_SHIFT_X = 22
TURN_TILT    = 6
TURN_SQUASH  = 0.15

# surprised
SURPRISED_SCALE_BASE  = 1.20
SURPRISED_SCALE_AMP   = 0.15
SURPRISED_BROW_AMP    = 4
SURPRISED_PUPIL_MIN   = 2
SURPRISED_PUPIL_SHRINK = 2
SURPRISED_MOUTH_BASE  = 4
SURPRISED_MOUTH_AMP   = 3

# angry
ANGRY_SCALE_BASE = 0.80
ANGRY_SCALE_AMP  = 0.10
ANGRY_PUPIL_DY   = 4

# love（爱心眼）
LOVE_DY_AMP       = 3
LOVE_HEART_SIZE   = 10

# dizzy（晕眩）
DIZZY_WOBBLE_AMP  = 4
DIZZY_SPIRAL_R    = 6

# cry（哭泣）
CRY_SCALE_BASE    = 0.70
CRY_SCALE_AMP     = 0.10
CRY_TEAR_SPEED    = 2.0

# shy（害羞）
SHY_SHIFT_X       = 6
SHY_SCALE_BASE    = 0.75
SHY_BLUSH_W       = 6

# cool（墨镜酷）
COOL_DY_AMP       = 1
COOL_GLASS_H      = 14

# thinking（思考）
THINK_PUPIL_DX    = 10
THINK_PUPIL_DY    = -4
THINK_DOT_Y       = 6

# excited（兴奋）
EXCITED_SCALE_BASE = 1.30
EXCITED_SCALE_AMP  = 0.20
EXCITED_DY_AMP     = 4
EXCITED_MOUTH_SIZE = 6

# wink（眨单眼）
WINK_DY_AMP       = 2

# yawn（打哈欠）
YAWN_MOUTH_BASE   = 3
YAWN_MOUTH_AMP    = 8
YAWN_SCALE_BASE   = 0.60
YAWN_SCALE_AMP    = 0.30

# confused（困惑）
CONFUSED_TILT      = 3
CONFUSED_L_SCALE   = 0.70
CONFUSED_R_SCALE   = 1.20

SCRIPT_DIR = Path(__file__).resolve().parent
SCRIPTS_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = SCRIPTS_DIR.parent

OLED_WIDTH = 128
OLED_HEIGHT = 64
FRAME_BUF_SIZE = OLED_WIDTH * OLED_HEIGHT // 8  # 1024

# 30fps 帧间隔
FRAME_MS = 33
FRAMES_PER_CYCLE = CYCLE_MS // FRAME_MS

# ---------------------------------------------------------------------------
# 缓动工具
# ---------------------------------------------------------------------------

def ease_in_out(t: float) -> float:
    return (1.0 - math.cos(t * math.pi)) / 2.0


def ping_pong_t(t: float) -> float:
    """输入 t ∈ [0,1]（周期内归一化时间），输出 [0,1] 缓动值"""
    if t > 0.5:
        t = 1.0 - t
    return ease_in_out(t * 2.0)


def bounce_px_t(t: float, amplitude: int) -> int:
    return round(ping_pong_t(t) * amplitude)


# 兼容旧的帧索引版本（GIF 用）
def ping_pong(frame_idx: int, total: int) -> float:
    t = (frame_idx % total) / max(total - 1, 1)
    if t > 0.5:
        t = 1.0 - t
    return ease_in_out(t * 2.0)


def bounce_px(frame_idx: int, total: int, amplitude: int) -> int:
    return round(ping_pong(frame_idx, total) * amplitude)


# ---------------------------------------------------------------------------
# 基础绘图工具
# ---------------------------------------------------------------------------

def rect(draw: ImageDraw.ImageDraw, xy: tuple[int, int, int, int]) -> None:
    draw.rectangle(xy, fill=255)


def rect_erase(draw: ImageDraw.ImageDraw, xy: tuple[int, int, int, int]) -> None:
    draw.rectangle(xy, fill=0)


def round_rect(
    draw: ImageDraw.ImageDraw, xy: tuple[int, int, int, int], radius: int = 3
) -> None:
    draw.rounded_rectangle(xy, radius=radius, fill=255)


def line(draw: ImageDraw.ImageDraw, xy: tuple[int, int, int, int], w: int = 1) -> None:
    draw.line(xy, fill=255, width=w)


def pupil(draw: ImageDraw.ImageDraw, cx: int, cy: int, r: int = PUPIL_R) -> None:
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=0)


# ---------------------------------------------------------------------------
# 眼睛绘制
# ---------------------------------------------------------------------------

def draw_eyes_with_pupils(
    draw: ImageDraw.ImageDraw,
    *,
    dy: int = 0,
    scale_h: float = 1.0,
    pupil_dx: int = 0,
    pupil_dy: int = 0,
    pupil_r: int = PUPIL_R,
    radius: int = EYE_RADIUS,
) -> None:
    hh = max(2, round(EYE_HH * scale_h))
    cy = EYE_CY + dy

    round_rect(draw, (EYE_L_CX - EYE_HW, cy - hh, EYE_L_CX + EYE_HW, cy + hh), radius=radius)
    round_rect(draw, (EYE_R_CX - EYE_HW, cy - hh, EYE_R_CX + EYE_HW, cy + hh), radius=radius)

    lpc_x = max(EYE_L_CX - EYE_HW + pupil_r + 1,
                min(EYE_L_CX + EYE_HW - pupil_r - 1, EYE_L_CX + pupil_dx))
    lpc_y = max(cy - hh + pupil_r + 1,
                min(cy + hh - pupil_r - 1, cy + pupil_dy))
    pupil(draw, lpc_x, lpc_y, pupil_r)

    rpc_x = max(EYE_R_CX - EYE_HW + pupil_r + 1,
                min(EYE_R_CX + EYE_HW - pupil_r - 1, EYE_R_CX + pupil_dx))
    rpc_y = max(cy - hh + pupil_r + 1,
                min(cy + hh - pupil_r - 1, cy + pupil_dy))
    pupil(draw, rpc_x, rpc_y, pupil_r)


def draw_eyes_blink(draw: ImageDraw.ImageDraw) -> None:
    line(draw, (EYE_L_CX - EYE_HW + 2, EYE_CY, EYE_L_CX + EYE_HW - 2, EYE_CY), w=3)
    line(draw, (EYE_R_CX - EYE_HW + 2, EYE_CY, EYE_R_CX + EYE_HW - 2, EYE_CY), w=3)


def draw_eyes_happy(draw: ImageDraw.ImageDraw, dy: int = 0) -> None:
    draw_eyes_with_pupils(draw, dy=dy)


# ---------------------------------------------------------------------------
# 嘴巴绘制
# ---------------------------------------------------------------------------

def draw_mouth_flat(draw: ImageDraw.ImageDraw) -> None:
    line(draw, (52, MOUTH_Y, 76, MOUTH_Y), w=2)


def draw_mouth_smile(draw: ImageDraw.ImageDraw, level: int = 2) -> None:
    y = MOUTH_Y
    line(draw, (46, y, 54, y + level), w=2)
    line(draw, (54, y + level, 74, y + level), w=2)
    line(draw, (74, y + level, 82, y), w=2)


def draw_mouth_sleepy(draw: ImageDraw.ImageDraw) -> None:
    line(draw, (56, MOUTH_Y, 72, MOUTH_Y), w=1)


def draw_mouth_surprised(draw: ImageDraw.ImageDraw, size: int = 5) -> None:
    cx, cy = 64, MOUTH_Y
    draw.ellipse((cx - size, cy - size // 2, cx + size, cy + size // 2), fill=255)


def draw_mouth_angry(draw: ImageDraw.ImageDraw) -> None:
    y = MOUTH_Y
    line(draw, (52, y + 2, 58, y), w=2)
    line(draw, (58, y, 70, y), w=2)
    line(draw, (70, y, 76, y + 2), w=2)


# ---------------------------------------------------------------------------
# 眉毛绘制
# ---------------------------------------------------------------------------

def draw_brows_angry(draw: ImageDraw.ImageDraw) -> None:
    line(draw, (EYE_L_CX - EYE_HW, 6, EYE_L_CX + EYE_HW, 10), w=2)
    line(draw, (EYE_R_CX - EYE_HW, 10, EYE_R_CX + EYE_HW, 6), w=2)


def draw_brows_surprised(draw: ImageDraw.ImageDraw, dy: int = 0) -> None:
    y = 4 - dy
    line(draw, (EYE_L_CX - EYE_HW + 2, y + 2, EYE_L_CX + EYE_HW - 2, y), w=2)
    line(draw, (EYE_R_CX - EYE_HW + 2, y, EYE_R_CX + EYE_HW - 2, y + 2), w=2)


def draw_heart(draw: ImageDraw.ImageDraw, cx: int, cy: int, size: int) -> None:
    """在 (cx, cy) 处绘制一个爱心"""
    s = max(3, size)
    hs = s // 2
    draw.ellipse((cx - s, cy - hs, cx, cy + hs), fill=255)
    draw.ellipse((cx, cy - hs, cx + s, cy + hs), fill=255)
    pts = [(cx - s, cy), (cx, cy + s), (cx + s, cy)]
    draw.polygon(pts, fill=255)


def draw_spiral_eye(draw: ImageDraw.ImageDraw, cx: int, cy: int, r: int) -> None:
    """绘制螺旋眼（晕眩用）"""
    round_rect(draw, (cx - EYE_HW, cy - EYE_HH, cx + EYE_HW, cy + EYE_HH), radius=EYE_RADIUS)
    line(draw, (cx - r, cy - r, cx + r, cy + r), w=2)
    line(draw, (cx + r, cy - r, cx - r, cy + r), w=2)


def draw_tear(draw: ImageDraw.ImageDraw, cx: int, y: int) -> None:
    """绘制一滴泪"""
    draw.ellipse((cx - 1, y, cx + 1, y + 3), fill=255)
    draw.point((cx, y - 1), fill=255)


def draw_blush(draw: ImageDraw.ImageDraw, cx: int, cy: int, w: int) -> None:
    """绘制腮红斜线"""
    for i in range(0, w, 2):
        line(draw, (cx - w // 2 + i, cy - 1, cx - w // 2 + i + 1, cy + 1), w=1)


def draw_sunglasses(draw: ImageDraw.ImageDraw, dy: int = 0) -> None:
    """绘制墨镜"""
    cy = EYE_CY + dy
    h = COOL_GLASS_H
    hh = h // 2
    rect(draw, (EYE_L_CX - EYE_HW - 2, cy - hh, EYE_L_CX + EYE_HW + 2, cy + hh))
    rect(draw, (EYE_R_CX - EYE_HW - 2, cy - hh, EYE_R_CX + EYE_HW + 2, cy + hh))
    line(draw, (EYE_L_CX + EYE_HW + 2, cy, EYE_R_CX - EYE_HW - 2, cy), w=2)
    line(draw, (EYE_L_CX - EYE_HW - 2, cy, EYE_L_CX - EYE_HW - 8, cy - 3), w=2)
    line(draw, (EYE_R_CX + EYE_HW + 2, cy, EYE_R_CX + EYE_HW + 8, cy - 3), w=2)
    rect_erase(draw, (EYE_L_CX - EYE_HW + 2, cy - hh + 2, EYE_L_CX + EYE_HW - 2, cy + hh - 2))
    rect_erase(draw, (EYE_R_CX - EYE_HW + 2, cy - hh + 2, EYE_R_CX + EYE_HW - 2, cy + hh - 2))


def draw_dots(draw: ImageDraw.ImageDraw, cx: int, cy: int, phase: float) -> None:
    """绘制思考省略号动画"""
    for i in range(3):
        offset = max(0.0, min(1.0, phase * 3.0 - i))
        r = 1 + round(offset)
        dx = (i - 1) * 8
        draw.ellipse((cx + dx - r, cy - r, cx + dx + r, cy + r), fill=255)


def draw_mouth_open(draw: ImageDraw.ImageDraw, size: int = 5) -> None:
    """绘制张大的嘴巴（兴奋/打哈欠用）"""
    cx, cy = 64, MOUTH_Y
    draw.ellipse((cx - size, cy - size, cx + size, cy + size // 2 + 1), fill=255)


def draw_question_mark(draw: ImageDraw.ImageDraw, cx: int, cy: int) -> None:
    """绘制问号"""
    draw.arc((cx - 4, cy - 8, cx + 4, cy), start=180, end=0, fill=255, width=2)
    line(draw, (cx + 4, cy - 4, cx, cy + 1), w=2)
    draw.point((cx, cy + 4), fill=255)


# ---------------------------------------------------------------------------
# 基于时间 t (0~1) 的表情绘制（用于生成 C 帧数据）
# ---------------------------------------------------------------------------

def render_expression_at_t(
    expression: str, t: float, width: int, height: int
) -> Image.Image:
    """根据归一化时间 t ∈ [0,1) 绘制一帧，嘴巴由 EXPR_HAS_MOUTH 控制"""
    img = Image.new("1", (width, height), 0)
    draw = ImageDraw.Draw(img)
    pp = ping_pong_t(t)
    has_mouth = EXPR_HAS_MOUTH.get(expression, False)

    if expression == "robot-idle":
        dy   = bounce_px_t(t, IDLE_DY_AMP)
        p_dy = bounce_px_t(t, IDLE_PUPIL_DY_AMP)
        draw_eyes_with_pupils(draw, dy=dy, pupil_dy=p_dy)

    elif expression == "robot-blink":
        if BLINK_CLOSE_START < t < BLINK_CLOSE_END:
            draw_eyes_blink(draw)
        else:
            draw_eyes_with_pupils(draw)

    elif expression == "robot-happy":
        dy = -bounce_px_t(t, HAPPY_DY_AMP)
        draw_eyes_happy(draw, dy=dy)
        smile_level = HAPPY_SMILE_BASE + round(pp * HAPPY_SMILE_AMP)
        draw_mouth_smile(draw, level=smile_level)

    elif expression == "robot-sleepy":
        if SLEEPY_CLOSE_START < t < SLEEPY_CLOSE_END:
            draw_eyes_blink(draw)
        else:
            scale = SLEEPY_SCALE_BASE + pp * SLEEPY_SCALE_AMP
            dy = round(pp * SLEEPY_DY_AMP)
            draw_eyes_with_pupils(draw, dy=dy, scale_h=scale, pupil_r=SLEEPY_PUPIL_R)
        if has_mouth:
            draw_mouth_sleepy(draw)

    elif expression in ("robot-turn-left", "robot-turn-right"):
        is_left = "left" in expression
        inertia_sign = 1 if is_left else -1
        shift_x = round(pp * TURN_SHIFT_X) * inertia_sign
        tilt = round(pp * TURN_TILT)
        l_dy = tilt if inertia_sign > 0 else -tilt
        r_dy = -l_dy
        squash = 1.0 - pp * TURN_SQUASH

        for (cx_base, dy_eye) in ((EYE_L_CX, l_dy), (EYE_R_CX, r_dy)):
            cx = cx_base + shift_x
            cy = EYE_CY + dy_eye
            hh = max(4, round(EYE_HH * squash))
            round_rect(draw, (cx - EYE_HW, cy - hh, cx + EYE_HW, cy + hh), radius=EYE_RADIUS)
            pupil(draw, cx, cy, PUPIL_R)

    elif expression == "robot-surprised":
        scale = SURPRISED_SCALE_BASE + pp * SURPRISED_SCALE_AMP
        brow_dy = round(pp * SURPRISED_BROW_AMP)
        p_r = max(SURPRISED_PUPIL_MIN, PUPIL_R - round(pp * SURPRISED_PUPIL_SHRINK))
        draw_brows_surprised(draw, dy=brow_dy)
        draw_eyes_with_pupils(draw, scale_h=scale, pupil_r=p_r)
        draw_mouth_surprised(draw, size=SURPRISED_MOUTH_BASE + round(pp * SURPRISED_MOUTH_AMP))

    elif expression == "robot-angry":
        p_dy = round(pp * ANGRY_PUPIL_DY)
        scale = ANGRY_SCALE_BASE - pp * ANGRY_SCALE_AMP
        draw_brows_angry(draw)
        draw_eyes_with_pupils(draw, scale_h=scale, pupil_dy=p_dy)
        draw_mouth_angry(draw)

    elif expression == "robot-love":
        dy = -bounce_px_t(t, LOVE_DY_AMP)
        heart_s = LOVE_HEART_SIZE + round(pp * 3)
        draw_heart(draw, EYE_L_CX, EYE_CY + dy, heart_s)
        draw_heart(draw, EYE_R_CX, EYE_CY + dy, heart_s)
        draw_mouth_smile(draw, level=4 + round(pp * 2))

    elif expression == "robot-dizzy":
        wobble = round(math.sin(t * math.pi * 4) * DIZZY_WOBBLE_AMP)
        draw_spiral_eye(draw, EYE_L_CX + wobble, EYE_CY, DIZZY_SPIRAL_R)
        draw_spiral_eye(draw, EYE_R_CX + wobble, EYE_CY, DIZZY_SPIRAL_R)
        draw_mouth_flat(draw)

    elif expression == "robot-cry":
        scale = CRY_SCALE_BASE + pp * CRY_SCALE_AMP
        draw_eyes_with_pupils(draw, scale_h=scale, pupil_dy=3)
        draw_mouth_flat(draw)
        tear_y_offset = round((t * CRY_TEAR_SPEED * 30) % 20)
        draw_tear(draw, EYE_L_CX - EYE_HW - 2, EYE_CY + tear_y_offset)
        draw_tear(draw, EYE_R_CX + EYE_HW + 2, EYE_CY + tear_y_offset)
        if tear_y_offset > 10:
            draw_tear(draw, EYE_L_CX - EYE_HW - 2, EYE_CY + tear_y_offset - 14)
            draw_tear(draw, EYE_R_CX + EYE_HW + 2, EYE_CY + tear_y_offset - 14)

    elif expression == "robot-shy":
        dx = round(pp * SHY_SHIFT_X)
        scale = SHY_SCALE_BASE + pp * 0.15
        draw_eyes_with_pupils(draw, scale_h=scale, pupil_dx=dx, pupil_dy=2)
        draw_blush(draw, EYE_L_CX - EYE_HW - 4, MOUTH_Y - 6, SHY_BLUSH_W)
        draw_blush(draw, EYE_R_CX + EYE_HW + 4, MOUTH_Y - 6, SHY_BLUSH_W)
        draw_mouth_flat(draw)

    elif expression == "robot-cool":
        dy = bounce_px_t(t, COOL_DY_AMP)
        draw_sunglasses(draw, dy=dy)
        y = MOUTH_Y + dy
        line(draw, (52, y, 58, y - 2), w=2)
        line(draw, (58, y - 2, 70, y - 2), w=2)
        line(draw, (70, y - 2, 76, y), w=2)

    elif expression == "robot-thinking":
        draw_eyes_with_pupils(draw, pupil_dx=THINK_PUPIL_DX, pupil_dy=THINK_PUPIL_DY)
        draw_dots(draw, EYE_R_CX + 10, THINK_DOT_Y, pp)
        draw_mouth_flat(draw)

    elif expression == "robot-excited":
        scale = EXCITED_SCALE_BASE + pp * EXCITED_SCALE_AMP
        dy = -bounce_px_t(t, EXCITED_DY_AMP)
        brow_dy = round(pp * 3)
        draw_brows_surprised(draw, dy=brow_dy)
        draw_eyes_with_pupils(draw, dy=dy, scale_h=scale, pupil_r=PUPIL_R + 1)
        draw_mouth_open(draw, size=EXCITED_MOUTH_SIZE + round(pp * 3))

    elif expression == "robot-wink":
        dy = bounce_px_t(t, WINK_DY_AMP)
        cy = EYE_CY + dy
        round_rect(draw, (EYE_R_CX - EYE_HW, cy - EYE_HH, EYE_R_CX + EYE_HW, cy + EYE_HH), radius=EYE_RADIUS)
        pupil(draw, EYE_R_CX, cy, PUPIL_R)
        line(draw, (EYE_L_CX - EYE_HW + 2, cy, EYE_L_CX + EYE_HW - 2, cy), w=3)
        draw_mouth_smile(draw, level=3)

    elif expression == "robot-yawn":
        phase = t
        if phase < 0.3:
            frac = phase / 0.3
            scale = 1.0 - frac * (1.0 - YAWN_SCALE_BASE)
            mouth = round(frac * YAWN_MOUTH_AMP)
        elif phase < 0.7:
            scale = YAWN_SCALE_BASE + ping_pong_t((phase - 0.3) / 0.4) * YAWN_SCALE_AMP
            mouth = YAWN_MOUTH_AMP
        else:
            frac = (phase - 0.7) / 0.3
            scale = YAWN_SCALE_BASE + (1.0 - frac) * YAWN_SCALE_AMP
            mouth = round((1.0 - frac) * YAWN_MOUTH_AMP)
        draw_eyes_with_pupils(draw, scale_h=max(0.2, scale), pupil_r=PUPIL_R - 1)
        draw_mouth_open(draw, size=YAWN_MOUTH_BASE + mouth)

    elif expression == "robot-confused":
        l_hh = max(4, round(EYE_HH * CONFUSED_L_SCALE))
        r_hh = max(4, round(EYE_HH * CONFUSED_R_SCALE))
        tilt = round(pp * CONFUSED_TILT)
        round_rect(draw, (EYE_L_CX - EYE_HW, EYE_CY - l_hh + tilt, EYE_L_CX + EYE_HW, EYE_CY + l_hh + tilt), radius=EYE_RADIUS)
        pupil(draw, EYE_L_CX, EYE_CY + tilt, PUPIL_R)
        round_rect(draw, (EYE_R_CX - EYE_HW, EYE_CY - r_hh - tilt, EYE_R_CX + EYE_HW, EYE_CY + r_hh - tilt), radius=EYE_RADIUS)
        pupil(draw, EYE_R_CX, EYE_CY - tilt, PUPIL_R)
        draw_question_mark(draw, EYE_R_CX + EYE_HW + 8, EYE_CY - EYE_HH + round(pp * 4))
        draw_mouth_flat(draw)

    else:
        raise ValueError(f"unknown expression: {expression}")

    return img


# ---------------------------------------------------------------------------
# PIL 图像 → SSD1306 页模式 framebuffer
# ---------------------------------------------------------------------------

def image_to_ssd1306_buf(img: Image.Image) -> bytes:
    """将 1-bit PIL 图像转为 SSD1306 页模式 1024 字节"""
    pixels = img.load()
    buf = bytearray(FRAME_BUF_SIZE)
    for page in range(8):
        for col in range(OLED_WIDTH):
            byte_val = 0
            for bit in range(8):
                y = page * 8 + bit
                if y < OLED_HEIGHT and pixels[col, y]:
                    byte_val |= (1 << bit)
            buf[page * OLED_WIDTH + col] = byte_val
    return bytes(buf)


# ---------------------------------------------------------------------------
# RLE 压缩（简单字节级 RLE：<count> <value>，count 1-128）
# ---------------------------------------------------------------------------

def rle_encode(data: bytes) -> bytes:
    """RLE 压缩，格式：重复 [count-1 | 0x80, value]，非重复 [count-1, byte0, byte1, ...]"""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        # 检查连续相同字节
        run_start = i
        while i < n and i - run_start < 128 and data[i] == data[run_start]:
            i += 1
        run_len = i - run_start
        if run_len >= 3:
            out.append((run_len - 1) | 0x80)
            out.append(data[run_start])
        else:
            # 收集非重复字节
            i = run_start
            lit_start = i
            while i < n and i - lit_start < 128:
                if i + 2 < n and data[i] == data[i + 1] == data[i + 2]:
                    break
                i += 1
            lit_len = i - lit_start
            if lit_len > 0:
                out.append(lit_len - 1)
                out.extend(data[lit_start:lit_start + lit_len])
    return bytes(out)


# ---------------------------------------------------------------------------
# 生成 C 帧数据头文件
# ---------------------------------------------------------------------------

ALL_EXPRESSIONS = [
    "robot-idle",
    "robot-blink",
    "robot-happy",
    "robot-sleepy",
    "robot-turn-left",
    "robot-turn-right",
    "robot-surprised",
    "robot-angry",
    "robot-love",
    "robot-dizzy",
    "robot-cry",
    "robot-shy",
    "robot-cool",
    "robot-thinking",
    "robot-excited",
    "robot-wink",
    "robot-yawn",
    "robot-confused",
]

EXPR_C_NAMES = {
    "robot-idle":       "idle",
    "robot-blink":      "blink",
    "robot-happy":      "happy",
    "robot-sleepy":     "sleepy",
    "robot-turn-left":  "turn_left",
    "robot-turn-right": "turn_right",
    "robot-surprised":  "surprised",
    "robot-angry":      "angry",
    "robot-love":       "love",
    "robot-dizzy":      "dizzy",
    "robot-cry":        "cry",
    "robot-shy":        "shy",
    "robot-cool":       "cool",
    "robot-thinking":   "thinking",
    "robot-excited":    "excited",
    "robot-wink":       "wink",
    "robot-yawn":       "yawn",
    "robot-confused":   "confused",
}

# 每个表情是否显示嘴巴（仅影响 sleepy 的嘴巴条件分支）
EXPR_HAS_MOUTH = {
    "robot-idle":       False,
    "robot-blink":      False,
    "robot-happy":      True,
    "robot-sleepy":     True,
    "robot-turn-left":  False,
    "robot-turn-right": False,
    "robot-surprised":  True,
    "robot-angry":      True,
    "robot-love":       True,
    "robot-dizzy":      True,
    "robot-cry":        True,
    "robot-shy":        True,
    "robot-cool":       True,
    "robot-thinking":   True,
    "robot-excited":    True,
    "robot-wink":       True,
    "robot-yawn":       True,
    "robot-confused":   True,
}


def generate_c_anim_header(
    expressions: list[str], width: int, height: int
) -> str:
    """生成包含所有表情 RLE 帧数据的 C 头文件"""
    lines: list[str] = [
        "#pragma once",
        "/* 自动生成，请勿手动修改。源：scripts/gif/generate_robot_faces.py */",
        "",
        "#include <stdint.h>",
        "",
        f"#define ANIM_CYCLE_MS   {CYCLE_MS}",
        f"#define ANIM_FRAME_MS   {FRAME_MS}",
        f"#define ANIM_FRAME_COUNT {FRAMES_PER_CYCLE}",
        "",
    ]

    total_bytes = 0

    for expr_name in expressions:
        c_name = EXPR_C_NAMES[expr_name]
        frame_arrays: list[str] = []

        for fi in range(FRAMES_PER_CYCLE):
            t = fi / FRAMES_PER_CYCLE
            img = render_expression_at_t(expr_name, t, width, height)
            raw = image_to_ssd1306_buf(img)
            compressed = rle_encode(raw)
            total_bytes += len(compressed)

            arr_name = f"s_anim_{c_name}_f{fi:02d}"
            lines.append(f"static const uint8_t {arr_name}[] = {{")
            # 每行 16 字节
            for chunk_start in range(0, len(compressed), 16):
                chunk = compressed[chunk_start:chunk_start + 16]
                hex_line = ", ".join(f"0x{b:02x}" for b in chunk)
                lines.append(f"    {hex_line},")
            lines.append("};")
            lines.append("")
            frame_arrays.append(arr_name)

        # 帧指针数组
        lines.append(f"static const uint8_t *const s_anim_{c_name}_frames[{FRAMES_PER_CYCLE}] = {{")
        for arr_name in frame_arrays:
            lines.append(f"    {arr_name},")
        lines.append("};")
        lines.append("")

        # 帧大小数组
        lines.append(f"static const uint16_t s_anim_{c_name}_sizes[{FRAMES_PER_CYCLE}] = {{")
        size_vals: list[str] = []
        for fi in range(FRAMES_PER_CYCLE):
            t = fi / FRAMES_PER_CYCLE
            img = render_expression_at_t(expr_name, t, width, height)
            raw = image_to_ssd1306_buf(img)
            compressed = rle_encode(raw)
            size_vals.append(str(len(compressed)))
        # 每行 8 个
        for chunk_start in range(0, len(size_vals), 8):
            chunk = size_vals[chunk_start:chunk_start + 8]
            lines.append(f"    {', '.join(chunk)},")
        lines.append("};")
        lines.append("")

    # 汇总表：按 EXPR_xxx 枚举顺序排列
    lines.append("/* 按 carmood_expr_t 枚举顺序排列 */")
    lines.append(f"static const uint8_t *const *const s_all_anim_frames[{len(expressions)}] = {{")
    for expr_name in expressions:
        c_name = EXPR_C_NAMES[expr_name]
        lines.append(f"    s_anim_{c_name}_frames,")
    lines.append("};")
    lines.append("")

    lines.append(f"static const uint16_t *const s_all_anim_sizes[{len(expressions)}] = {{")
    for expr_name in expressions:
        c_name = EXPR_C_NAMES[expr_name]
        lines.append(f"    s_anim_{c_name}_sizes,")
    lines.append("};")
    lines.append("")

    print(f"  帧数据总大小: {total_bytes} 字节 ({total_bytes / 1024:.1f} KB)")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# GIF 生成（兼容旧的帧索引模式，少量帧用于预览）
# ---------------------------------------------------------------------------

def make_frame(
    width: int, height: int, expression: str, frame_idx: int
) -> Image.Image:
    """基于帧索引的绘制（GIF 预览用，帧数较少）"""
    n = 6
    t = (frame_idx % n) / max(n - 1, 1)
    return render_expression_at_t(expression, t, width, height)


def generate_expression(
    out_root: Path,
    gif_root: Path,
    expression: str,
    frames: int,
    width: int,
    height: int,
    duration_ms: int,
) -> None:
    expr_dir = out_root / expression
    expr_dir.mkdir(parents=True, exist_ok=True)

    seq: list[Image.Image] = []
    for i in range(frames):
        img = make_frame(width, height, expression, i)
        png_path = expr_dir / f"frame-{i + 1:02d}.png"
        img.save(png_path)
        seq.append(img.convert("P"))

    gif_root.mkdir(parents=True, exist_ok=True)
    gif_path = gif_root / f"{expression}.gif"
    seq[0].save(
        gif_path,
        save_all=True,
        append_images=seq[1:],
        duration=duration_ms,
        loop=0,
        optimize=False,
        disposal=2,
    )
    print(f"生成 {expression}: {frames} 帧 → {gif_path}")


# ---------------------------------------------------------------------------
# 高帧率 GIF（30fps，与 C 端帧数据完全一致的预览）
# ---------------------------------------------------------------------------

def generate_hq_gif(
    gif_root: Path, expression: str, width: int, height: int
) -> None:
    """生成 30fps 高帧率 GIF，和 OLED 播放效果一致"""
    gif_root.mkdir(parents=True, exist_ok=True)
    seq: list[Image.Image] = []
    for fi in range(FRAMES_PER_CYCLE):
        t = fi / FRAMES_PER_CYCLE
        img = render_expression_at_t(expression, t, width, height)
        seq.append(img.convert("P"))

    gif_path = gif_root / f"{expression}-hq.gif"
    seq[0].save(
        gif_path,
        save_all=True,
        append_images=seq[1:],
        duration=FRAME_MS,
        loop=0,
        optimize=False,
        disposal=2,
    )
    print(f"生成高帧率 {expression}: {FRAMES_PER_CYCLE} 帧 → {gif_path}")


# ---------------------------------------------------------------------------
# 入口
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="生成机器人脸表情帧序列与 GIF + C 帧数据")
    p.add_argument("--width",    type=int, default=128)
    p.add_argument("--height",   type=int, default=64)
    p.add_argument("--frames",   type=int, default=8, help="GIF 预览帧数")
    p.add_argument("--duration", type=int, default=120, help="GIF 帧间隔 ms")
    p.add_argument(
        "--out",
        type=Path,
        default=PROJECT_ROOT / "assets" / "expressions",
        help="PNG 序列输出目录",
    )
    p.add_argument(
        "--gif-out",
        type=Path,
        default=PROJECT_ROOT / "assets" / "gifs",
        help="GIF 输出目录",
    )
    p.add_argument(
        "--expressions",
        nargs="+",
        default=None,
        help="只生成指定表情（默认全部）",
    )
    p.add_argument(
        "--no-c-header",
        action="store_true",
        help="不生成 C 帧数据头文件",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    targets = args.expressions if args.expressions else ALL_EXPRESSIONS

    # 生成低帧率 GIF 预览
    for name in targets:
        generate_expression(
            out_root=args.out,
            gif_root=args.gif_out,
            expression=name,
            frames=args.frames,
            width=args.width,
            height=args.height,
            duration_ms=args.duration,
        )

    # 生成 30fps 高帧率 GIF 预览
    for name in targets:
        generate_hq_gif(args.gif_out, name, args.width, args.height)

    # 生成 C 帧数据头文件
    if not args.no_c_header:
        print("\n生成 C 帧数据头文件...")
        header_content = generate_c_anim_header(
            targets, args.width, args.height
        )
        header_path = PROJECT_ROOT / "firmware" / "main" / "ui" / "face_anim_data.h"
        header_path.write_text(header_content, encoding="utf-8")
        print(f"→ {header_path}")

    print("\n全部完成")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
