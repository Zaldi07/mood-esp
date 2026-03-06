#!/usr/bin/env python3
"""
把 assets/expressions/<name>/frame-XX.png 序列转成
firmware/main/assets/anim_<safe_name>.h

位图格式与 ssd1306_oled.c 内部 framebuffer 一致：
  - 128×64，共 8 页，每页 128 列
  - byte[page * 128 + col] 的 bit[row] 对应像素 (col, page*8+row)
  - 1 = 亮，0 = 灭
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent

EXPR_ROOT = PROJECT_ROOT / "assets" / "expressions"
OUT_ROOT  = PROJECT_ROOT / "firmware" / "main" / "assets"

OLED_W = 128
OLED_H = 64
BYTES_PER_FRAME = OLED_W * OLED_H // 8  # 1024


def png_to_framebuf(path: Path, invert: bool) -> bytes:
    """将单帧 PNG 转为 OLED 页模式 1024 字节"""
    img = Image.open(path).convert("1")
    if img.size != (OLED_W, OLED_H):
        img = img.resize((OLED_W, OLED_H), Image.NEAREST)

    buf = bytearray(BYTES_PER_FRAME)
    for page in range(8):
        for col in range(OLED_W):
            byte = 0
            for bit in range(8):
                y = page * 8 + bit
                pixel = img.getpixel((col, y))
                # PIL "1" 模式：255=白，0=黑
                on = (pixel != 0) ^ invert
                if on:
                    byte |= (1 << bit)
            buf[page * OLED_W + col] = byte
    return bytes(buf)


def safe_name(expr_name: str) -> str:
    """robot-turn-left → robot_turn_left"""
    return re.sub(r"[^a-zA-Z0-9]", "_", expr_name)


def generate_header(expr_name: str, frames_data: list[bytes], out_dir: Path) -> Path:
    sname = safe_name(expr_name)
    guard = f"ASSETS_ANIM_{sname.upper()}_H"
    n_frames = len(frames_data)

    lines: list[str] = [
        f"#pragma once",
        f"/* 自动生成，请勿手动修改。源：assets/expressions/{expr_name}/ */",
        f"#include <stdint.h>",
        f"",
        f"#define ANIM_{sname.upper()}_FRAME_COUNT {n_frames}",
        f"#define ANIM_{sname.upper()}_FRAME_BYTES {BYTES_PER_FRAME}",
        f"",
        f"static const uint8_t anim_{sname}[{n_frames}][{BYTES_PER_FRAME}] = {{",
    ]

    for fi, data in enumerate(frames_data):
        lines.append(f"    /* frame {fi + 1} */")
        lines.append("    {")
        # 每行 16 字节
        for row_start in range(0, BYTES_PER_FRAME, 16):
            chunk = data[row_start:row_start + 16]
            hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
            lines.append(f"        {hex_str},")
        lines.append("    },")

    lines += ["};", ""]

    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"anim_{sname}.h"
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return out_path


def process_expression(expr_dir: Path, out_dir: Path, invert: bool) -> None:
    pngs = sorted(expr_dir.glob("frame-*.png"))
    if not pngs:
        print(f"  跳过 {expr_dir.name}：没有 frame-*.png")
        return

    frames_data = [png_to_framebuf(p, invert) for p in pngs]
    out_path = generate_header(expr_dir.name, frames_data, out_dir)
    print(f"  {expr_dir.name}: {len(pngs)} 帧 → {out_path.relative_to(PROJECT_ROOT)}")


def main() -> int:
    p = argparse.ArgumentParser(description="PNG 序列 → OLED C 数组头文件")
    p.add_argument(
        "--invert", action="store_true",
        help="反色（PNG 白色 → OLED 灭，黑色 → 亮）"
    )
    p.add_argument(
        "--expr-root", type=Path, default=EXPR_ROOT,
        help="表情 PNG 序列根目录"
    )
    p.add_argument(
        "--out", type=Path, default=OUT_ROOT,
        help="C 头文件输出目录"
    )
    p.add_argument(
        "expressions", nargs="*",
        help="只处理指定表情名（默认全部）"
    )
    args = p.parse_args()

    expr_dirs = sorted(args.expr_root.iterdir()) if args.expr_root.exists() else []
    expr_dirs = [d for d in expr_dirs if d.is_dir()]
    if args.expressions:
        expr_dirs = [d for d in expr_dirs if d.name in args.expressions]

    if not expr_dirs:
        print(f"未找到任何表情目录：{args.expr_root}")
        return 1

    print(f"输出目录: {args.out}")
    for d in expr_dirs:
        process_expression(d, args.out, args.invert)
    print("全部完成")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
