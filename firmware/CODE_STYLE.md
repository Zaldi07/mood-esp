# Firmware Code Style and Structure

## 1. Style baseline
This project uses **Google C++ style** for `firmware/main` C/C++ sources.

- Formatter: `clang-format`
- Config file: project root `.clang-format`
- Check command:
  - `scripts/check_firmware_style.sh --check`
- Auto-fix command:
  - `scripts/check_firmware_style.sh --fix`

## 2. Commit gate
Enable local pre-commit hook once:

```bash
git config core.hooksPath .githooks
```

After that, each commit runs style checks automatically.

## 3. main.c split rule
Keep `app_main()` focused on:
- init order
- task start
- high-level state transition

Move implementation details into modules.

### Recommended target structure

```text
firmware/main/
  app/
    app_main.c
    app_main.h
    app_state.c
    app_state.h
  drivers/
    lis3dh.c
    lis3dh.h
    ssd1306_oled.c
    ssd1306_oled.h
    touch_input.c
    touch_input.h
  ui/
    oled_font5x7.c
    oled_font5x7.h
    oled_overlay.c
    oled_overlay.h
    pattern_renderer.c
    pattern_renderer.h
  main.c
  CMakeLists.txt
```

### Mapping from current `firmware/main/main.c`

- `lis3dh_*` -> `drivers/lis3dh.c`
- `touch_init` + touch debounce/click logic -> `drivers/touch_input.c`
- `get_glyph_5x7`/`oled_draw_*` -> `ui/oled_font5x7.c` + `ui/oled_overlay.c`
- `draw_checkerboard`/`draw_stripes`/`draw_border`/`show_pattern` -> `ui/pattern_renderer.c`
- Calibration flow (`lis3dh_run_guided_calibration`) -> `app/app_state.c` (or `services/calibration.c`)

## 4. Migration sequence (low risk)
1. Move OLED font and pattern functions first (no hardware side effects).
2. Move LIS3DH driver read/write/init APIs.
3. Move touch sampling + click/long-press state machine.
4. Keep `main.c` as orchestrator only; final size target: `< 200` lines.

## 5. Naming and file constraints
- One module = one public header + one source file.
- Public API in `*.h`; file-local helpers must be `static`.
- Avoid cross-module global writable state; expose setters/getters if needed.
- New files must pass style check before commit.
