#!/usr/bin/env bash
set -euo pipefail

MODE="${1:---check}"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Please install clang-format first." >&2
  exit 127
fi

mapfile -t FILES < <(rg --files firmware/main -g '*.[ch]')
if [[ ${#FILES[@]} -eq 0 ]]; then
  echo "No C/C++ source files found under firmware/main"
  exit 0
fi

case "$MODE" in
  --fix)
    clang-format -style=file -i "${FILES[@]}"
    echo "Formatted ${#FILES[@]} files."
    ;;
  --check)
    clang-format -style=file --dry-run --Werror "${FILES[@]}"
    echo "Style check passed for ${#FILES[@]} files."
    ;;
  *)
    echo "Usage: $0 [--check|--fix]" >&2
    exit 2
    ;;
esac
