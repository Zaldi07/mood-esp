1. 不要直接手改 u8g2_font_my_chinese.c 里的大数组。
2. 这个目录用于存放 astra-lite 的中文字体生成输入和输出，风格对齐 astra-launcher。
3. chars.txt 是当前字符清单，str.map 是给 bdfconv 用的映射文件。
4. 默认用 /Users/jzh/Projects/carmood/scripts/generate_astra_lite_font.py 重新生成字体。
5. 只更新 chars.txt 和 str.map:
6. uv run python3 /Users/jzh/Projects/carmood/scripts/generate_astra_lite_font.py --source /Users/jzh/Projects/carmood/firmware/main/ui/astra_menu_bridge.c --include-regex 'astra_new_|oled_draw_UTF8|snprintf\(' --chars-out /Users/jzh/Projects/carmood/firmware/main/ui/astra_lite/Others/chars.txt --map-out /Users/jzh/Projects/carmood/firmware/main/ui/astra_lite/Others/str.map
7. 连同字体 C 文件一起生成（必须使用 demo 目录里编译的 bdfconv 和 wenquanyi_9pt.bdf）:
8. uv run python3 /Users/jzh/Projects/carmood/scripts/generate_astra_lite_font.py --source /Users/jzh/Projects/carmood/firmware/main/ui/astra_menu_bridge.c --include-regex 'astra_new_|oled_draw_UTF8|snprintf\(' --chars-out /Users/jzh/Projects/carmood/firmware/main/ui/astra_lite/Others/chars.txt --map-out /Users/jzh/Projects/carmood/firmware/main/ui/astra_lite/Others/str.map --bdfconv /Users/jzh/Projects/carmood/demo/astra-launcher/Others/bdfconv/bdfconv --bdf /Users/jzh/Projects/carmood/demo/astra-launcher/Others/wenquanyi_9pt.bdf --font-out /Users/jzh/Projects/carmood/firmware/main/ui/astra_lite/Others/u8g2_font_my_chinese.c
9. 重要：不要使用外部安装的 bdfconv 或其他版本——必须使用 demo/astra-launcher/Others/bdfconv/ 下的源码编译版本，否则生成的字体 unicode lookup table 格式与项目 u8g2 库不兼容。
10. 如果 bdfconv 不存在，先在 demo/astra-launcher/Others/bdfconv/ 目录下运行 make 或 cc -o bdfconv main.c bdf_font.c bdf_glyph.c bdf_kern.c bdf_map.c bdf_parser.c bdf_rle.c bdf_tga.c bdf_8x8.c fd.c -lm 编译。
11. 默认会提取菜单项、状态页 UTF-8 文案和 snprintf 里的中文格式串，避免漏字。
12. 如需扫描更多 UI 字符串，可追加 --source /absolute/path/to/file.c，并按需要调整 --include-regex。
13. 生成完成后，输出文件保持为 u8g2_font_my_chinese.c，主工程会直接编译这个文件。
14. 当前项目运行时通过 ui/astra_lite/astra_ui_draw_driver.h 中的 u8g2_font_my_chinese 符号引用该字体。
