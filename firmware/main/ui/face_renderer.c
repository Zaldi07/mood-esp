#include "ui/face_renderer.h"
#include "ui/face_anim_data.h"

#include <string.h>

#include "ssd1306_oled.h"

#define FACE_BUF_SIZE (OLED_WIDTH * OLED_HEIGHT / 8)
#define REACTION_EYE_MAX_X 7
#define REACTION_EYE_MAX_Y 5
#define REACTION_SHAKE_X_DIV 120
#define REACTION_SHAKE_Y_DIV 140

/* 动画任务单线程使用的工作缓冲，避免在 4KB 任务栈上反复申请大数组。 */
static uint8_t s_face_frame_buf[FACE_BUF_SIZE];
static uint8_t s_face_from_buf[FACE_BUF_SIZE];
static uint8_t s_face_to_buf[FACE_BUF_SIZE];
static uint8_t s_face_blend_buf[FACE_BUF_SIZE];
static int32_t s_reaction_eye_x_q8 = 0;
static int32_t s_reaction_eye_y_q8 = 0;

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

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void draw_hline(int x, int y, int width)
{
    for (int i = 0; i < width; i++) {
        oled_set_pixel(x + i, y, true);
    }
}

static void draw_vline(int x, int y, int height)
{
    for (int i = 0; i < height; i++) {
        oled_set_pixel(x, y + i, true);
    }
}

static void draw_rect(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    draw_hline(x, y, width);
    draw_hline(x, y + height - 1, width);
    draw_vline(x, y, height);
    draw_vline(x + width - 1, y, height);
}

static void fill_rect(int x, int y, int width, int height)
{
    for (int py = 0; py < height; py++) {
        draw_hline(x, y + py, width);
    }
}

static void draw_line(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y0 < y1 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        oled_set_pixel(x0, y0, true);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void fill_circle(int cx, int cy, int radius)
{
    int rr = radius * radius;
    for (int py = -radius; py <= radius; py++) {
        for (int px = -radius; px <= radius; px++) {
            if (px * px + py * py <= rr) {
                oled_set_pixel(cx + px, cy + py, true);
            }
        }
    }
}

static void draw_eye_box(int x, int y, int width, int height)
{
    draw_rect(x, y, width, height);
    draw_rect(x + 1, y + 1, width - 2, height - 2);
}

static void draw_pupil(int center_x, int center_y, int eye_dx, int eye_dy, int radius)
{
    fill_circle(center_x + eye_dx, center_y + eye_dy, radius);
    oled_set_pixel(center_x + eye_dx + radius + 1, center_y + eye_dy, true);
}

static void draw_dizzy_face(int eye_dx, int eye_dy, int mouth_jitter)
{
    draw_eye_box(19, 17, 34, 22);
    draw_eye_box(75, 17, 34, 22);
    draw_pupil(36, 28, eye_dx, eye_dy, 4);
    draw_pupil(92, 28, eye_dx, eye_dy, 4);
    draw_line(52, 48 + mouth_jitter, 76, 49 + mouth_jitter);
    draw_line(54, 51 + mouth_jitter, 74, 51 + mouth_jitter);
}

static void draw_angry_dizzy_face(int eye_dx, int eye_dy, int mouth_jitter)
{
    draw_eye_box(18, 19, 36, 18);
    draw_eye_box(74, 19, 36, 18);
    draw_line(18, 12, 52, 18);
    draw_line(76, 18, 110, 12);
    draw_line(18, 13, 52, 19);
    draw_line(76, 19, 110, 13);
    draw_pupil(36, 28, eye_dx, eye_dy, 4);
    draw_pupil(92, 28, eye_dx, eye_dy, 4);
    draw_line(50, 51 + mouth_jitter, 58, 47 + mouth_jitter);
    draw_line(58, 47 + mouth_jitter, 64, 49 + mouth_jitter);
    draw_line(64, 49 + mouth_jitter, 70, 47 + mouth_jitter);
    draw_line(70, 47 + mouth_jitter, 78, 51 + mouth_jitter);
    fill_rect(59, 50 + mouth_jitter, 10, 2);
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

void face_render_motion_reaction(carmood_reaction_mode_t mode,
                                 int32_t shake_lr,
                                 int32_t shake_fb,
                                 int32_t shake_val,
                                 uint32_t t_ms)
{
    static const int8_t s_orbit_x[12] = {0, 1, 2, 1, 0, -1, -2, -1, 0, 1, 2, 1};
    static const int8_t s_orbit_y[12] = {-1, -2, -1, 0, 1, 2, 1, 0, -1, -2, -1, 0};

    int eye_max_y = (mode == CARMOOD_REACTION_ANGRY_DIZZY) ? 4 : REACTION_EYE_MAX_Y;
    int target_eye_x = clamp_int((int)(shake_lr / REACTION_SHAKE_X_DIV),
                                 -REACTION_EYE_MAX_X, REACTION_EYE_MAX_X);
    int target_eye_y = clamp_int((int)(shake_fb / REACTION_SHAKE_Y_DIV),
                                 -eye_max_y, eye_max_y);
    int orbit_amp = 0;
    int orbit_idx = (int)((t_ms / 60U) % 12U);
    int mouth_jitter = 0;

    if (t_ms < 80U) {
        s_reaction_eye_x_q8 = 0;
        s_reaction_eye_y_q8 = 0;
    }

    s_reaction_eye_x_q8 += ((target_eye_x << 8) - s_reaction_eye_x_q8) / 4;
    s_reaction_eye_y_q8 += ((target_eye_y << 8) - s_reaction_eye_y_q8) / 4;

    if (shake_val >= 2600) {
        orbit_amp = 1;
    }
    if (shake_val >= 3400 || mode == CARMOOD_REACTION_ANGRY_DIZZY) {
        orbit_amp = 2;
    }

    int eye_dx = s_reaction_eye_x_q8 >> 8;
    int eye_dy = s_reaction_eye_y_q8 >> 8;
    eye_dx += (s_orbit_x[orbit_idx] * orbit_amp) / 2;
    eye_dy += (s_orbit_y[orbit_idx] * orbit_amp) / 2;
    eye_dx = clamp_int(eye_dx, -REACTION_EYE_MAX_X, REACTION_EYE_MAX_X);
    eye_dy = clamp_int(eye_dy, -eye_max_y, eye_max_y);

    if (mode == CARMOOD_REACTION_ANGRY_DIZZY) {
        mouth_jitter = ((int)(t_ms / 70U) & 0x01) ? 1 : 0;
    }

    oled_clear_buf();
    if (mode == CARMOOD_REACTION_ANGRY_DIZZY) {
        draw_angry_dizzy_face(eye_dx, eye_dy, mouth_jitter);
    } else {
        draw_dizzy_face(eye_dx, eye_dy, 0);
    }
}
