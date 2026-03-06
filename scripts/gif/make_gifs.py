#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent
SCRIPTS_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = SCRIPTS_DIR.parent


def parse_size(value: str) -> tuple[int, int]:
    parts = value.lower().split("x", 1)
    if len(parts) != 2:
        raise argparse.ArgumentTypeError("尺寸格式应为 128x64")
    try:
        width = int(parts[0])
        height = int(parts[1])
    except ValueError as exc:
        raise argparse.ArgumentTypeError("尺寸必须是整数，例如 128x64") from exc
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("尺寸必须为正数")
    return width, height


def to_1bit_frame(path: Path, size: tuple[int, int], threshold: int, invert: bool) -> Image.Image:
    with Image.open(path) as img:
        gray = img.convert("L")
        resized = gray.resize(size, Image.Resampling.NEAREST)
        if invert:
            bw = resized.point(lambda px: 0 if px >= threshold else 255, mode="1")
        else:
            bw = resized.point(lambda px: 255 if px >= threshold else 0, mode="1")
        return bw.convert("P")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="从表情帧目录批量生成 GIF。")
    parser.add_argument(
        "--src",
        default=str(PROJECT_ROOT / "assets" / "expressions"),
        help="帧图根目录",
    )
    parser.add_argument(
        "--out",
        default=str(PROJECT_ROOT / "assets" / "gifs"),
        help="GIF 输出目录",
    )
    parser.add_argument("--pattern", default="frame-*.png", help="帧图匹配模式")
    parser.add_argument("--size", type=parse_size, default=(128, 64), help="统一缩放尺寸 WxH")
    parser.add_argument("--delay", type=int, default=12, help="帧延迟（1/100 秒）")
    parser.add_argument("--loop", type=int, default=0, help="循环次数，0 表示无限循环")
    parser.add_argument("--threshold", type=int, default=55, help="黑白阈值（0-100）")
    parser.add_argument("--invert", action="store_true", help="反相输出（白底黑字转黑底白字）")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    if args.delay < 0:
        print("错误: --delay 必须 >= 0", file=sys.stderr)
        return 2
    if args.loop < 0:
        print("错误: --loop 必须 >= 0", file=sys.stderr)
        return 2
    if not 0 <= args.threshold <= 100:
        print("错误: --threshold 必须在 0..100", file=sys.stderr)
        return 2

    src_root = Path(args.src)
    out_root = Path(args.out)
    if not src_root.is_dir():
        print(f"错误: 未找到帧图目录: {src_root}", file=sys.stderr)
        return 1

    out_root.mkdir(parents=True, exist_ok=True)

    processed = 0
    skipped = 0
    threshold_255 = int(args.threshold * 255 / 100)

    for expr_dir in sorted(src_root.iterdir()):
        if not expr_dir.is_dir():
            continue

        frame_paths = sorted(expr_dir.glob(args.pattern))
        if not frame_paths:
            print(f"跳过 {expr_dir.name}: 没有匹配到 '{args.pattern}'")
            skipped += 1
            continue

        frames = [to_1bit_frame(path, args.size, threshold_255, args.invert) for path in frame_paths]
        first, rest = frames[0], frames[1:]
        out_gif = out_root / f"{expr_dir.name}.gif"
        first.save(
            out_gif,
            save_all=True,
            append_images=rest,
            duration=args.delay * 10,
            loop=args.loop,
            optimize=False,
            disposal=2,
        )
        print(f"已生成: {out_gif}（{len(frame_paths)} 帧）")
        processed += 1

    if processed == 0:
        print(f"没有生成 GIF。请把帧图放在 '{src_root}/<expression>/{args.pattern}'。")
        return 1

    print(f"完成。processed={processed} skipped={skipped} out={out_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
