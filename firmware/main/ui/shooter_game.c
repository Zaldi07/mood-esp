#include "ui/shooter_game.h"

#include <string.h>
#include <stdlib.h>
#include "esp_timer.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"

#define W OLED_WIDTH
#define H OLED_HEIGHT

/* 玩家飞机参数 */
#define PLAYER_W      7
#define PLAYER_H      8
#define PLAYER_Y      (H - PLAYER_H - 1)
#define PLAYER_SPEED  3

/* 子弹参数 */
#define MAX_BULLETS   8
#define BULLET_SPEED  3
#define AUTO_FIRE_INTERVAL_MS 200

/* 敌机参数 */
#define MAX_ENEMIES       8
#define ENEMY_W           6
#define ENEMY_H           6
#define ENEMY_BASE_SPEED  1
#define SPAWN_INTERVAL_MS 1200

/* 无敌闪烁 */
#define INVINCIBLE_MS     1500

/* 爆炸动画 */
#define MAX_EXPLOSIONS    4
#define EXPLOSION_FRAMES  6
#define EXPLOSION_FRAME_MS 50

/* 星星背景 */
#define MAX_STARS 16

typedef struct {
    int x, y;
    bool active;
} bullet_t;

typedef struct {
    int x, y;
    int speed;
    int hp;
    bool active;
} enemy_t;

typedef struct {
    int x, y;
    int frame;
    bool active;
    int64_t start_us;
} explosion_t;

typedef struct {
    int x, y;
    int speed;
} star_t;

/* 游戏状态 */
static int       s_player_x;
static int       s_score;
static int       s_lives;
static bool      s_game_over;
static int64_t   s_last_fire_us;
static int64_t   s_last_spawn_us;
static int64_t   s_last_tick_us;
static int       s_spawn_interval_ms;
static int       s_difficulty;
static int       s_move_dir;          /* -1=左, 0=停, 1=右 */
static int64_t   s_invincible_until;  /* 受伤后无敌截止时间(us) */

static bullet_t    s_bullets[MAX_BULLETS];
static enemy_t     s_enemies[MAX_ENEMIES];
static explosion_t s_explosions[MAX_EXPLOSIONS];
static star_t      s_stars[MAX_STARS];

/* 待处理输入 */
static volatile shooter_input_t s_pending_input;

/* 简易伪随机 */
static uint32_t s_rand_state;
static uint32_t game_rand(void)
{
    s_rand_state ^= s_rand_state << 13;
    s_rand_state ^= s_rand_state >> 17;
    s_rand_state ^= s_rand_state << 5;
    return s_rand_state;
}

/* ------------------------------------------------------------------ */
/* 5x7 字形（分数显示用）                                                */
/* ------------------------------------------------------------------ */

static const uint8_t s_glyph_num[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
};

static void draw_glyph(int x, int y, const uint8_t glyph[5])
{
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++) {
            if (!((glyph[col] >> row) & 1)) continue;
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < W && py >= 0 && py < H)
                oled_draw_pixel(px, py);
        }
}

static void draw_number(int x, int y, int num)
{
    char buf[8];
    int len = 0;
    if (num <= 0) {
        buf[0] = '0'; len = 1;
    } else {
        int tmp = num;
        while (tmp > 0 && len < 7) {
            buf[len++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        for (int i = 0; i < len / 2; i++) {
            char c = buf[i]; buf[i] = buf[len - 1 - i]; buf[len - 1 - i] = c;
        }
    }
    for (int i = 0; i < len; i++) {
        int digit = buf[i] - '0';
        draw_glyph(x + i * 6, y, s_glyph_num[digit]);
    }
}

/* ------------------------------------------------------------------ */
/* 绘制精灵                                                             */
/* ------------------------------------------------------------------ */

/* 玩家飞机（7x8 像素，三角形造型） */
static void draw_player(int x, int y)
{
    /*     *
         * *
        *   *
       ** * **
      *  ***  *
      *  ***  *
      * ***** *
       *******     */
    oled_draw_pixel(x + 3, y);
    oled_draw_pixel(x + 2, y + 1);
    oled_draw_pixel(x + 4, y + 1);
    oled_draw_pixel(x + 1, y + 2);
    oled_draw_pixel(x + 5, y + 2);
    oled_draw_pixel(x + 0, y + 3); oled_draw_pixel(x + 1, y + 3);
    oled_draw_pixel(x + 3, y + 3);
    oled_draw_pixel(x + 5, y + 3); oled_draw_pixel(x + 6, y + 3);
    oled_draw_pixel(x + 0, y + 4); oled_draw_pixel(x + 3, y + 4);
    oled_draw_pixel(x + 2, y + 4); oled_draw_pixel(x + 4, y + 4);
    oled_draw_pixel(x + 6, y + 4);
    oled_draw_pixel(x + 0, y + 5); oled_draw_pixel(x + 2, y + 5);
    oled_draw_pixel(x + 3, y + 5); oled_draw_pixel(x + 4, y + 5);
    oled_draw_pixel(x + 6, y + 5);
    oled_draw_pixel(x + 0, y + 6);
    for (int i = 1; i <= 5; i++) oled_draw_pixel(x + i, y + 6);
    oled_draw_pixel(x + 6, y + 6);
    for (int i = 1; i <= 5; i++) oled_draw_pixel(x + i, y + 7);
}

/* 敌机（6x6 像素，倒三角造型） */
static void draw_enemy(int x, int y)
{
    /*  ******
       * ** *
        *  *
        ****
         **
          *     */
    for (int i = 0; i < 6; i++) oled_draw_pixel(x + i, y);
    oled_draw_pixel(x + 0, y + 1); oled_draw_pixel(x + 2, y + 1);
    oled_draw_pixel(x + 3, y + 1); oled_draw_pixel(x + 5, y + 1);
    oled_draw_pixel(x + 1, y + 2); oled_draw_pixel(x + 4, y + 2);
    for (int i = 1; i <= 4; i++) oled_draw_pixel(x + i, y + 3);
    oled_draw_pixel(x + 2, y + 4); oled_draw_pixel(x + 3, y + 4);
    oled_draw_pixel(x + 3, y + 5);
}

/* 爆炸效果 */
static void draw_explosion(int cx, int cy, int frame)
{
    int r = frame + 1;
    /* 十字 + 对角线散射 */
    for (int i = -r; i <= r; i++) {
        int px, py;
        px = cx + i; py = cy;
        if (px >= 0 && px < W && py >= 0 && py < H) oled_draw_pixel(px, py);
        px = cx; py = cy + i;
        if (px >= 0 && px < W && py >= 0 && py < H) oled_draw_pixel(px, py);
    }
    if (frame >= 2) {
        int dr = r - 1;
        for (int d = -dr; d <= dr; d++) {
            int px, py;
            px = cx + d; py = cy + d;
            if (px >= 0 && px < W && py >= 0 && py < H) oled_draw_pixel(px, py);
            px = cx + d; py = cy - d;
            if (px >= 0 && px < W && py >= 0 && py < H) oled_draw_pixel(px, py);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 初始化                                                               */
/* ------------------------------------------------------------------ */

void shooter_game_init(void)
{
    s_player_x = (W - PLAYER_W) / 2;
    s_score = 0;
    s_lives = 5;
    s_game_over = false;
    s_last_fire_us = 0;
    s_last_spawn_us = esp_timer_get_time();
    s_last_tick_us = esp_timer_get_time();
    s_spawn_interval_ms = SPAWN_INTERVAL_MS;
    s_difficulty = 0;
    s_move_dir = 0;
    s_invincible_until = 0;
    s_pending_input = SHOOTER_INPUT_NONE;
    s_rand_state = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFF);
    if (s_rand_state == 0) s_rand_state = 12345;

    memset(s_bullets, 0, sizeof(s_bullets));
    memset(s_enemies, 0, sizeof(s_enemies));
    memset(s_explosions, 0, sizeof(s_explosions));

    /* 初始化星星背景 */
    for (int i = 0; i < MAX_STARS; i++) {
        s_stars[i].x = game_rand() % W;
        s_stars[i].y = game_rand() % H;
        s_stars[i].speed = 1 + (game_rand() % 2);
    }
}

void shooter_game_push_input(shooter_input_t input)
{
    s_pending_input = input;
}

/* ------------------------------------------------------------------ */
/* 游戏逻辑                                                             */
/* ------------------------------------------------------------------ */

static void spawn_enemy(void)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (s_enemies[i].active) continue;
        s_enemies[i].active = true;
        s_enemies[i].x = game_rand() % (W - ENEMY_W);
        s_enemies[i].y = -ENEMY_H;
        s_enemies[i].speed = ENEMY_BASE_SPEED + (s_difficulty / 3);
        if (s_enemies[i].speed > 4) s_enemies[i].speed = 4;
        s_enemies[i].hp = 1;
        break;
    }
}

static void spawn_explosion(int x, int y)
{
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (s_explosions[i].active) continue;
        s_explosions[i].active = true;
        s_explosions[i].x = x;
        s_explosions[i].y = y;
        s_explosions[i].frame = 0;
        s_explosions[i].start_us = esp_timer_get_time();
        break;
    }
}

static void fire_bullet(void)
{
    int64_t now = esp_timer_get_time();
    if ((now - s_last_fire_us) / 1000 < AUTO_FIRE_INTERVAL_MS) return;
    s_last_fire_us = now;

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (s_bullets[i].active) continue;
        s_bullets[i].active = true;
        s_bullets[i].x = s_player_x + PLAYER_W / 2;
        s_bullets[i].y = PLAYER_Y - 2;
        break;
    }
}

static bool rect_overlap(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void player_take_hit(int64_t now)
{
    if (now < s_invincible_until) return;
    s_lives--;
    if (s_lives <= 0) {
        s_game_over = true;
        return;
    }
    s_invincible_until = now + (int64_t)INVINCIBLE_MS * 1000;
}

static void update_game(void)
{
    int64_t now = esp_timer_get_time();

    /* 处理输入：更新持续移动方向 */
    shooter_input_t input = s_pending_input;
    s_pending_input = SHOOTER_INPUT_NONE;

    switch (input) {
    case SHOOTER_INPUT_LEFT:
        s_player_x -= PLAYER_SPEED;
        if (s_player_x < 0) s_player_x = 0;
        break;
    case SHOOTER_INPUT_RIGHT:
        s_player_x += PLAYER_SPEED;
        if (s_player_x > W - PLAYER_W) s_player_x = W - PLAYER_W;
        break;
    case SHOOTER_INPUT_LEFT_HOLD:
        s_move_dir = -1;
        break;
    case SHOOTER_INPUT_RIGHT_HOLD:
        s_move_dir = 1;
        break;
    case SHOOTER_INPUT_RELEASE:
        s_move_dir = 0;
        break;
    case SHOOTER_INPUT_FIRE:
        fire_bullet();
        break;
    default:
        break;
    }

    /* 持续移动 */
    if (s_move_dir != 0) {
        s_player_x += s_move_dir * PLAYER_SPEED;
        if (s_player_x < 0) s_player_x = 0;
        if (s_player_x > W - PLAYER_W) s_player_x = W - PLAYER_W;
    }

    /* 自动开火 */
    if ((now - s_last_fire_us) / 1000 >= AUTO_FIRE_INTERVAL_MS) {
        fire_bullet();
    }

    /* 更新子弹 */
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!s_bullets[i].active) continue;
        s_bullets[i].y -= BULLET_SPEED;
        if (s_bullets[i].y < -2) s_bullets[i].active = false;
    }

    /* 更新敌机 */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!s_enemies[i].active) continue;
        s_enemies[i].y += s_enemies[i].speed;
        /* 敌机飞出屏幕底部直接消失，不扣命 */
        if (s_enemies[i].y > H + ENEMY_H) {
            s_enemies[i].active = false;
        }
    }

    /* 子弹-敌机碰撞检测 */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!s_bullets[b].active) continue;
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!s_enemies[e].active) continue;
            if (rect_overlap(s_bullets[b].x - 1, s_bullets[b].y, 2, 3,
                             s_enemies[e].x, s_enemies[e].y, ENEMY_W, ENEMY_H)) {
                s_bullets[b].active = false;
                s_enemies[e].hp--;
                if (s_enemies[e].hp <= 0) {
                    spawn_explosion(s_enemies[e].x + ENEMY_W / 2,
                                    s_enemies[e].y + ENEMY_H / 2);
                    s_enemies[e].active = false;
                    s_score += 10;
                    s_difficulty = s_score / 50;
                    s_spawn_interval_ms = SPAWN_INTERVAL_MS - s_difficulty * 60;
                    if (s_spawn_interval_ms < 300) s_spawn_interval_ms = 300;
                }
                break;
            }
        }
    }

    /* 玩家-敌机碰撞检测（无敌期间跳过） */
    bool invincible = (now < s_invincible_until);
    if (!invincible) {
        for (int e = 0; e < MAX_ENEMIES; e++) {
            if (!s_enemies[e].active) continue;
            if (rect_overlap(s_player_x, PLAYER_Y, PLAYER_W, PLAYER_H,
                             s_enemies[e].x, s_enemies[e].y, ENEMY_W, ENEMY_H)) {
                spawn_explosion(s_enemies[e].x + ENEMY_W / 2,
                                s_enemies[e].y + ENEMY_H / 2);
                s_enemies[e].active = false;
                player_take_hit(now);
                if (s_game_over) return;
            }
        }
    }

    /* 生成新敌机 */
    if ((now - s_last_spawn_us) / 1000 >= (int64_t)s_spawn_interval_ms) {
        s_last_spawn_us = now;
        spawn_enemy();
    }

    /* 更新爆炸动画 */
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!s_explosions[i].active) continue;
        int elapsed = (int)((now - s_explosions[i].start_us) / 1000);
        s_explosions[i].frame = elapsed / EXPLOSION_FRAME_MS;
        if (s_explosions[i].frame >= EXPLOSION_FRAMES) {
            s_explosions[i].active = false;
        }
    }

    /* 更新星星 */
    for (int i = 0; i < MAX_STARS; i++) {
        s_stars[i].y += s_stars[i].speed;
        if (s_stars[i].y >= H) {
            s_stars[i].y = 0;
            s_stars[i].x = game_rand() % W;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 渲染                                                                 */
/* ------------------------------------------------------------------ */

static void render_game(void)
{
    oled_clear_buffer();

    /* 星星背景 */
    for (int i = 0; i < MAX_STARS; i++) {
        oled_draw_pixel(s_stars[i].x, s_stars[i].y);
    }

    /* 玩家（无敌时闪烁） */
    int64_t now = esp_timer_get_time();
    bool invincible = (now < s_invincible_until);
    bool draw_player_flag = !invincible || ((now / 80000) & 1);
    if (draw_player_flag) {
        draw_player(s_player_x, PLAYER_Y);
    }

    /* 子弹 */
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!s_bullets[i].active) continue;
        oled_draw_pixel(s_bullets[i].x, s_bullets[i].y);
        oled_draw_pixel(s_bullets[i].x, s_bullets[i].y + 1);
    }

    /* 敌机 */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!s_enemies[i].active) continue;
        draw_enemy(s_enemies[i].x, s_enemies[i].y);
    }

    /* 爆炸 */
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!s_explosions[i].active) continue;
        draw_explosion(s_explosions[i].x, s_explosions[i].y,
                       s_explosions[i].frame);
    }

    /* HUD: 左上角分数，右上角生命 */
    draw_number(1, 1, s_score);

    /* 生命值用小心形表示 */
    for (int i = 0; i < s_lives; i++) {
        int hx = W - 6 - i * 7;
        oled_draw_pixel(hx + 1, 1); oled_draw_pixel(hx + 2, 1);
        oled_draw_pixel(hx + 4, 1); oled_draw_pixel(hx + 5, 1);
        for (int c = 0; c < 6; c++) oled_draw_pixel(hx + c, 2);
        for (int c = 0; c < 6; c++) oled_draw_pixel(hx + c, 3);
        oled_draw_pixel(hx + 1, 4); oled_draw_pixel(hx + 2, 4);
        oled_draw_pixel(hx + 3, 4); oled_draw_pixel(hx + 4, 4);
        oled_draw_pixel(hx + 2, 5); oled_draw_pixel(hx + 3, 5);
        oled_draw_pixel(hx + 3, 6);
    }

    oled_send_buffer();
}

static void render_game_over(void)
{
    oled_clear_buffer();

    /* "GAME OVER" 用中文字体 */
    oled_set_font(u8g2_font_my_chinese);
    oled_set_draw_color(1);
    oled_draw_UTF8(37, 24, "GAME OVER");

    /* 分数 */
    draw_number((W - 6 * 4) / 2, 34, s_score);

    /* 提示 */
    oled_draw_UTF8(28, 58, "Hold to Exit");

    oled_send_buffer();
}

/* ------------------------------------------------------------------ */
/* 公开接口                                                             */
/* ------------------------------------------------------------------ */

bool shooter_game_tick(void)
{
    if (s_game_over) {
        render_game_over();
        return false;
    }

    update_game();
    render_game();
    return !s_game_over;
}

int shooter_game_get_score(void)
{
    return s_score;
}

bool shooter_game_is_over(void)
{
    return s_game_over;
}
