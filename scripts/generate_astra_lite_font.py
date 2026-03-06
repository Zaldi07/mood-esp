#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"', re.DOTALL)


def extract_ordered_chinese_chars(paths: list[Path], include_regex: str | None) -> list[str]:
    seen: set[str] = set()
    ordered: list[str] = []
    include_pattern = re.compile(include_regex) if include_regex else None

    for path in paths:
        text = path.read_text(encoding="utf-8")
        if include_pattern:
            for line in text.splitlines():
                if not include_pattern.search(line):
                    continue
                for match in STRING_RE.finditer(line):
                    literal = match.group(0)[1:-1]
                    for ch in literal:
                        if "\u4e00" <= ch <= "\u9fff" and ch not in seen:
                            seen.add(ch)
                            ordered.append(ch)
            continue

        for match in STRING_RE.finditer(text):
            literal = match.group(0)[1:-1]
            for ch in literal:
                if "\u4e00" <= ch <= "\u9fff" and ch not in seen:
                    seen.add(ch)
                    ordered.append(ch)

    return ordered


def write_chars_file(path: Path, chars: list[str]) -> None:
    path.write_text("".join(chars) + "\n", encoding="utf-8")


def write_map_file(path: Path, chars: list[str], ascii_range: str) -> None:
    encoded = [f"${ord(ch):x}" for ch in chars]
    line = ascii_range
    if encoded:
        line += "," + ",".join(encoded)
    path.write_text(line + "\n", encoding="ascii")


def patch_generated_font_source(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    include_line = '#include "libs/u8g2/u8g2.h"\n'
    if include_line in text:
        return
    path.write_text(include_line + text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Extract Chinese glyphs from source files and generate u8g2 str.map."
    )
    parser.add_argument(
        "--source",
        dest="sources",
        action="append",
        required=True,
        help="UTF-8 source file to scan. Can be specified multiple times.",
    )
    parser.add_argument(
        "--chars-out",
        required=True,
        help="Output path for the ordered Chinese character list.",
    )
    parser.add_argument(
        "--map-out",
        required=True,
        help="Output path for the generated u8g2 str.map file.",
    )
    parser.add_argument(
        "--ascii-range",
        default="32-128",
        help="ASCII range prefix for str.map. Default: 32-128.",
    )
    parser.add_argument(
        "--include-regex",
        default=None,
        help="Optional regex. If set, only scan matching lines.",
    )
    parser.add_argument(
        "--bdfconv",
        default=None,
        help="Optional path to bdfconv. If set together with --bdf, generate the font C file.",
    )
    parser.add_argument(
        "--bdf",
        default=None,
        help="Optional path to the source .bdf font file.",
    )
    parser.add_argument(
        "--font-out",
        default=None,
        help="Optional output path for generated font C file.",
    )
    parser.add_argument(
        "--font-name",
        default="u8g2_font_my_chinese",
        help="Generated font symbol name. Default: u8g2_font_my_chinese.",
    )
    args = parser.parse_args()

    source_paths = [Path(p).resolve() for p in args.sources]
    missing = [str(path) for path in source_paths if not path.is_file()]
    if missing:
        raise SystemExit("Missing source file(s): " + ", ".join(missing))

    chars = extract_ordered_chinese_chars(source_paths, args.include_regex)
    write_chars_file(Path(args.chars_out), chars)
    write_map_file(Path(args.map_out), chars, args.ascii_range)

    print(f"Scanned {len(source_paths)} source file(s)")
    print(f"Extracted {len(chars)} unique Chinese glyph(s)")
    print(f"Wrote {args.chars_out}")
    print(f"Wrote {args.map_out}")

    if args.bdfconv or args.bdf or args.font_out:
        if not (args.bdfconv and args.bdf and args.font_out):
            raise SystemExit("--bdfconv, --bdf, and --font-out must be provided together")

        bdfconv = Path(args.bdfconv).resolve()
        bdf = Path(args.bdf).resolve()
        font_out = Path(args.font_out).resolve()

        if not bdfconv.is_file():
            raise SystemExit(f"bdfconv not found: {bdfconv}")
        if not bdf.is_file():
            raise SystemExit(f"BDF font file not found: {bdf}")

        cmd = [
            str(bdfconv),
            "-v",
            "-b",
            "0",
            "-f",
            "1",
            "-M",
            str(Path(args.map_out).resolve()),
            "-n",
            args.font_name,
            "-o",
            str(font_out),
            str(bdf),
        ]
        subprocess.run(cmd, check=True)
        patch_generated_font_source(font_out)
        print(f"Wrote {font_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
