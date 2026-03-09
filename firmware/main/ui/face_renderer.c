#include "ui/face_renderer.h"
#include "ui/face_anim_data.h"

#include <string.h>

#include "ssd1306_oled.h"

#define FACE_BUF_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)

/* 动画任务单线程使用的工作缓冲，避免在 4KB 任务栈上反复申请大数组。 */
static uint8_t s_face_frame_buf[FACE_BUF_SIZE];
static uint8_t s_face_from_buf[FACE_BUF_SIZE];
static uint8_t s_face_to_buf[FACE_BUF_SIZE];
static uint8_t s_face_blend_buf[FACE_BUF_SIZE];

static const uint8_t s_bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

/* ------------------------------------------------------------------ */
/* RLE 解压 → 直接写入 OLED framebuffer                                */
/* ------------------------------------------------------------------ */

static void rle_decode_to_buf(uint8_t *dst_buf, const uint8_t *src, uint16_t src_len)
{
    int dst = 0;
    int i = 0;
    const int buf_size = FACE_BUF_SIZE;

    while (i < src_len && dst < buf_size) {
        uint8_t header = src[i++];
        if (header & 0x80) {
            /* 重复：count = (header & 0x7F) + 1, 后跟 1 字节 */
            int count = (header & 0x7F) + 1;
            if (i >= src_len) break;
            uint8_t val = src[i++];
            for (int j = 0; j < count && dst < buf_size; j++)
                dst_buf[dst++] = val;
        } else {
            /* 非重复：count = header + 1, 后跟 count 字节 */
            int count = header + 1;
            for (int j = 0; j < count && i < src_len && dst < buf_size; j++)
                dst_buf[dst++] = src[i++];
        }
    }

    /* 不足部分填 0 */
    while (dst < buf_size)
        dst_buf[dst++] = 0;
}

static void decode_expr_frame(uint8_t *dst_buf, carmood_expr_t expr, uint32_t t_ms)
{
    if (expr >= EXPR_COUNT) expr = EXPR_IDLE;

    uint32_t frame_in_cycle = (t_ms % ANIM_CYCLE_MS) / ANIM_FRAME_MS;
    if (frame_in_cycle >= ANIM_FRAME_COUNT)
        frame_in_cycle = ANIM_FRAME_COUNT - 1;

    const uint8_t *frame_data = s_all_anim_frames[expr][frame_in_cycle];
    uint16_t frame_size = s_all_anim_sizes[expr][frame_in_cycle];

    rle_decode_to_buf(dst_buf, frame_data, frame_size);
}

static bool buf_get_pixel(const uint8_t *buf, int x, int y)
{
    uint16_t idx = (uint16_t)((y / 8) * OLED_WIDTH + x);
    uint8_t bit = (uint8_t)(1U << (y % 8));
    return (buf[idx] & bit) != 0;
}

static void buf_set_pixel(uint8_t *buf, int x, int y, bool on)
{
    uint16_t idx = (uint16_t)((y / 8) * OLED_WIDTH + x);
    uint8_t bit = (uint8_t)(1U << (y % 8));
    if (on) {
        buf[idx] |= bit;
    } else {
        buf[idx] &= (uint8_t)~bit;
    }
}

static void blend_transition(uint8_t *dst_buf,
                             const uint8_t *from_buf,
                             const uint8_t *to_buf,
                             uint8_t progress_255)
{
    memset(dst_buf, 0, FACE_BUF_SIZE);

    int progress = progress_255 * (OLED_WIDTH * 16 + 15) / 255;
    for (int y = 0; y < OLED_HEIGHT; y++) {
        for (int x = 0; x < OLED_WIDTH; x++) {
            int threshold = x * 16 / OLED_WIDTH + s_bayer4x4[y & 0x03][x & 0x03];
            bool use_new = progress >= threshold;
            bool pixel_on = use_new ? buf_get_pixel(to_buf, x, y)
                                    : buf_get_pixel(from_buf, x, y);
            buf_set_pixel(dst_buf, x, y, pixel_on);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 公开接口                                                             */
/* ------------------------------------------------------------------ */

void face_render_frame(carmood_expr_t expr, uint32_t t_ms)
{
    decode_expr_frame(s_face_frame_buf, expr, t_ms);
    oled_draw_bitmap(s_face_frame_buf);
}

void face_render_transition(carmood_expr_t from_expr,
                            uint32_t from_t_ms,
                            carmood_expr_t to_expr,
                            uint32_t to_t_ms,
                            uint8_t progress_255)
{
    if (progress_255 == 0) {
        face_render_frame(from_expr, from_t_ms);
        return;
    }
    if (progress_255 >= 255) {
        face_render_frame(to_expr, to_t_ms);
        return;
    }

    decode_expr_frame(s_face_from_buf, from_expr, from_t_ms);
    decode_expr_frame(s_face_to_buf, to_expr, to_t_ms);
    blend_transition(s_face_blend_buf, s_face_from_buf, s_face_to_buf, progress_255);
    oled_draw_bitmap(s_face_blend_buf);
}
