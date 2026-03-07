# Layout Studio

PyQt6-based OLED layout editor for `128x64` Carmood pages.

## Run

```bash
cd /Users/jzh/Projects/carmood/layout-studio
uv run python main.py
```

## Current Scope

- Add `text`, `line`, `rect`
- Edit coordinates and size
- Live preview in a scaled `128x64` canvas
- Export JSON
- Export C header compatible with `layout_renderer.h`

## Default Header Export Target

Recommended export path:

`/Users/jzh/Projects/carmood/firmware/main/ui/layout_default_data.h`
