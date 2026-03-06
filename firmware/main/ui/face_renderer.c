#include "ui/face_renderer.h"
#include "ui/face_anim_data.h"

#include <string.h>

#include "ssd1306_oled.h"

/* ------------------------------------------------------------------ */
/* RLE 解压 → 直接写入 OLED framebuffer                                */
/* ------------------------------------------------------------------ */

static void rle_decode_to_framebuf(const uint8_t *src, uint16_t src_len)
{
    uint8_t buf[OLED_WIDTH * OLED_HEIGHT / 8];
    int dst = 0;
    int i = 0;
    const int buf_size = sizeof(buf);

    while (i < src_len && dst < buf_size) {
        uint8_t header = src[i++];
        if (header & 0x80) {
            /* 重复：count = (header & 0x7F) + 1, 后跟 1 字节 */
            int count = (header & 0x7F) + 1;
            if (i >= src_len) break;
            uint8_t val = src[i++];
            for (int j = 0; j < count && dst < buf_size; j++)
                buf[dst++] = val;
        } else {
            /* 非重复：count = header + 1, 后跟 count 字节 */
            int count = header + 1;
            for (int j = 0; j < count && i < src_len && dst < buf_size; j++)
                buf[dst++] = src[i++];
        }
    }

    /* 不足部分填 0 */
    while (dst < buf_size)
        buf[dst++] = 0;

    oled_draw_bitmap(buf);
}

/* ------------------------------------------------------------------ */
/* 公开接口                                                             */
/* ------------------------------------------------------------------ */

void face_render_frame(carmood_expr_t expr, uint32_t t_ms)
{
    if (expr >= EXPR_COUNT) expr = EXPR_IDLE;

    /* 根据时间选帧 */
    uint32_t frame_in_cycle = (t_ms % ANIM_CYCLE_MS) / ANIM_FRAME_MS;
    if (frame_in_cycle >= ANIM_FRAME_COUNT)
        frame_in_cycle = ANIM_FRAME_COUNT - 1;

    const uint8_t *frame_data = s_all_anim_frames[expr][frame_in_cycle];
    uint16_t frame_size = s_all_anim_sizes[expr][frame_in_cycle];

    rle_decode_to_framebuf(frame_data, frame_size);
}
