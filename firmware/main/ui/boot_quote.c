#include "ui/boot_quote.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drivers/touch_input.h"
#include "ssd1306_oled.h"
#include "ui/astra_lite/astra_ui_draw_driver.h"

static const char* TAG = "boot_quote";

#define MAX_WRAPPED_LINES 36
#define VISIBLE_LINES 4
#define LINE_HEIGHT 12
#define BASE_Y 24
#define MAX_TEXT_WIDTH 122

/* 26 quotes from R.F. Kuang's "Babel, or the Necessity of Violence" */
static const char* const s_babel_quotes[BOOT_QUOTE_COUNT] = {
    /* 1 */
    "\"We have to die to get their pity. We have to die for them to find us noble. "
    "Our deaths are thus great acts of rebellion, a wretched lament that highlights their "
    "inhumanity. "
    "Our deaths become their battle cry. I don't want to be their Imoinda, their Oroonoko. "
    "I don't want to be their tragic, lovely lacquer figure. I want to live.\"",

    /* 2 */
    "\"That's the beauty of learning a new language. It should feel like an enormous undertaking. "
    "It ought to intimidate you. It makes you appreciate the complexity of the ones you know "
    "already.\"",

    /* 3 */
    "\"Nice comes from the Latin word for 'stupid,' said Griffin. 'We do not want to be nice.\"",

    /* 4 */
    "\"After all, we're here to make the unknown known, to make the other familiar. "
    "We're here to make magic with words.\"",

    /* 5 */
    "\"History isn't a premade tapestry that we've got to suffer, a closed world with no exit. "
    "We can form it. Make it. We just have to choose to make it.\"",

    /* 6 */
    "\"They were men at Oxford; they were not Oxford men. But the enormity of this knowledge "
    "was so devastating, such a vicious antithesis to the three golden days they'd blindly "
    "enjoyed, "
    "that neither of them could say it out loud.\"",

    /* 7 */
    "\"Betrayal. Translation means doing violence upon the original, means warping and distorting "
    "it for foreign, unintended eyes. So then where does that leave us? How can we conclude, "
    "except by acknowledging that an act of translation is then necessarily always an act of "
    "betrayal?\"",

    /* 8 */
    "\"The thing about violence, see, is that the Empire has a lot more to lose than we do. "
    "Violence disrupts the extractive economy. You wreak havoc on one supply line, and there's a "
    "dip "
    "in prices across the Atlantic. Their entire system of trade is high-strung and vulnerable to "
    "shocks "
    "because they've made it thus, because the rapacious greed of capitalism is punishing. "
    "It's why slave revolts succeed. They can't fire on their own source of labour - "
    "it'd be like killing their own golden geese.\"",

    /* 9 */
    "\"Power did not lie in the tip of a pen. Power did not work against its own interests. "
    "Power could only be brought to heel by acts of defiance it could not ignore. "
    "With brute, unflinching force. With violence.\"",

    /* 10 */
    "\"Books are meant to be touched, otherwise they're useless.\"",

    /* 11 */
    "\"The word loss was inadequate. Loss just meant a lack, meant something was missing, "
    "but it did not encompass the totality of this severance, this terrifying un-anchoring "
    "from all that he'd ever known.\"",

    /* 12 */
    "\"So, you see, translators do not so much deliver a message as they rewrite the original. "
    "And herein lies the difficulty - rewriting is still writing, and writing always reflects "
    "the author's ideology and biases.\"",

    /* 13 */
    "\"Grief suffocated. Grief paralysed. Grief was a cruel, heavy boot pressed so hard against "
    "his chest that he could not breathe.\"",

    /* 14 */
    "\"English did not just borrow words from other languages; it was stuffed to the brim "
    "with foreign influences, a Frankenstein vernacular. And Robin found it incredible, "
    "how this country, whose citizens prided themselves so much on being better than the rest "
    "of the world, could not make it through an afternoon tea without borrowed goods.\"",

    /* 15 */
    "\"Empire needed extraction. Violence shocked the system, because the system could not "
    "cannibalize "
    "itself and survive. The hands of the Empire were tied, because it could not raze that "
    "from which it profited.\"",

    /* 16 */
    "\"That's just what translation is, I think. That's all speaking is. Listening to the other "
    "and trying to see past your own biases to glimpse what they're trying to say. "
    "Showing yourself to the world, and hoping someone else understands.\"",

    /* 17 */
    "\"Nice comes from the Latin word for 'stupid,' said Griffin. We do not want to be nice.\"",

    /* 18 */
    "\"But that's the great contradiction of colonialism.' Cathy uttered this like a simple matter "
    "of fact. 'It's built to destroy that which it prizes most.'\"",

    /* 19 */
    "\"History isn't a pre-made tapestry that we've got to suffer, a closed world with no exit. "
    "We can form it. Make it. We just have to choose to make it.\"",

    /* 20 */
    "\"He hated this place. He loved it. He resented how it treated him. He still wanted to be a "
    "part "
    "of it - because it felt so good to be a part of it, to speak to its professors as an "
    "intellectual "
    "equal, to be in on the great game.\"",

    /* 21 */
    "\"That's just what translation is, I think. That's all speaking is. Listening to the other "
    "and trying to see past your own biases to glimpse what they're trying to say. "
    "Showing yourself to the world, and hoping someone else understands.\"",

    /* 22 */
    "\"A lie was not a lie if it was never uttered; questions that were never asked did not need "
    "answers. "
    "They would both remain perfectly content to linger in the liminal, endless space between "
    "truth and denial.\"",

    /* 23 */
    "\"She learned revolution is, in fact, always unimaginable. It shatters the world you know. "
    "The future is unwritten, brimming with potential. The colonizers have no idea what is coming, "
    "and that makes them panic. It terrifies them.\"",

    /* 24 */
    "\"Anger was a chokehold. Anger did not empower you. It sat on your chest; it squeezed your "
    "ribs "
    "until you felt trapped, suffocated, out of options. Anger simmered, then exploded. "
    "Anger was constriction, and the consequent rage a desperate attempt to breathe.\"",

    /* 25 */
    "\"If we push in the right spots - then we've moved things to the breaking point. "
    "Then the future becomes fluid, and change is possible. History isn't a pre-made tapestry "
    "that we've got to suffer, a closed world with no exit. We can form it. Make it. "
    "We just have to choose to make it.\"",

    /* 26 */
    "\"But never forget the audacity of what you are attempting. "
    "Never forget that you are defying a curse laid by God.\""};

typedef struct {
  uint16_t start;
  uint16_t len;
} quote_line_t;

/* RTC memory persists across soft resets and sleep */
static RTC_DATA_ATTR uint32_t s_last_quote_index = 0;

uint32_t boot_quote_get_count(void) {
  return BOOT_QUOTE_COUNT;
}

const char* boot_quote_get_text(uint32_t index) {
  if (index >= BOOT_QUOTE_COUNT) {
    index = 0;
  }
  return s_babel_quotes[index];
}

uint32_t boot_quote_get_next_index(void) {
  uint32_t rnd = (uint32_t)(esp_random() % BOOT_QUOTE_COUNT);
  if (rnd == s_last_quote_index) {
    rnd = (rnd + 1) % BOOT_QUOTE_COUNT;
  }
  s_last_quote_index = rnd;
  return rnd;
}

static int split_quote_lines(const char* text, quote_line_t* lines, int max_lines) {
  int text_len = (int)strlen(text);
  int count = 0;
  int i = 0;

  while (i < text_len && count < max_lines) {
    while (i < text_len && text[i] == ' ') {
      i++;
    }
    if (i >= text_len) break;
    int cur_start = i;
    int last_break = -1;
    char temp[96];

    while (i <= text_len) {
      if (i == text_len || text[i] == ' ') {
        int seg_len = i - cur_start;
        if (seg_len >= sizeof(temp)) seg_len = sizeof(temp) - 1;
        memcpy(temp, &text[cur_start], seg_len);
        temp[seg_len] = '\0';

        int width = oled_get_str_width(temp);
        if (width <= MAX_TEXT_WIDTH) {
          last_break = i;
          if (i == text_len) break;
        } else {
          break;
        }
      }
      i++;
    }

    if (last_break > cur_start) {
      lines[count].start = (uint16_t)cur_start;
      lines[count].len = (uint16_t)(last_break - cur_start);
      count++;
      i = last_break;
    } else {
      int seg_len = i - cur_start;
      if (seg_len <= 0) seg_len = 1;
      lines[count].start = (uint16_t)cur_start;
      lines[count].len = (uint16_t)seg_len;
      count++;
    }
  }

  return count;
}

void boot_quote_run_intro(void) {
  ESP_LOGI(TAG, "Starting boot quote intro animation");

  astra_ui_driver_init();
  oled_set_font(u8g2_font_my_chinese);

  uint32_t quote_idx = boot_quote_get_next_index();
  const char* quote_str = boot_quote_get_text(quote_idx);
  int total_chars = (int)strlen(quote_str);

  quote_line_t lines[MAX_WRAPPED_LINES];
  int line_count = split_quote_lines(quote_str, lines, MAX_WRAPPED_LINES);

  int chars_revealed = 0;
  int chars_per_frame = (total_chars > 200) ? 2 : 1;
  float scroll_y = 0.0f;
  int target_scroll_line = 0;
  bool is_typing_done = false;
  int reading_pause_frames = 110; /* ~3.5 seconds at 30 fps */
  int frame = 0;

  char badge[16];
  snprintf(badge, sizeof(badge), "#%02lu/26", (unsigned long)(quote_idx + 1));
  int badge_w = oled_get_str_width(badge);

  while (1) {
    /* Check touch input to allow fast-forward or instant skip */
    touch_input_event_t touch_event = {};
    if (touch_input_poll(&touch_event) == ESP_OK) {
      touch_input_key_event_t top_key = touch_event.keys[TOUCH_KEY_TOP];
      touch_input_key_event_t up_key = touch_event.keys[TOUCH_KEY_UP];
      touch_input_key_event_t down_key = touch_event.keys[TOUCH_KEY_DOWN];

      if (top_key.valid_click || touch_event.samples[TOUCH_KEY_TOP].stable_pressed) {
        if (!is_typing_done) {
          /* First tap during typing: complete text instantly */
          chars_revealed = total_chars;
          is_typing_done = true;
          if (line_count > VISIBLE_LINES) {
            target_scroll_line = line_count - VISIBLE_LINES;
            scroll_y = (float)(target_scroll_line * LINE_HEIGHT);
          }
        } else {
          /* Tap when already complete: exit intro immediately */
          ESP_LOGI(TAG, "Boot quote skipped by user");
          break;
        }
      }

      /* Allow manual scrolling if user interacts with up/down touch sensors */
      if (up_key.valid_click || touch_event.swipe == TOUCH_SWIPE_UP) {
        if (target_scroll_line > 0) target_scroll_line--;
      }
      if (down_key.valid_click || touch_event.swipe == TOUCH_SWIPE_DOWN) {
        if (target_scroll_line < line_count - VISIBLE_LINES) target_scroll_line++;
      }
    }

    /* Advance typewriter state */
    if (!is_typing_done) {
      chars_revealed += chars_per_frame;
      if (chars_revealed >= total_chars) {
        chars_revealed = total_chars;
        is_typing_done = true;
      }
    } else {
      reading_pause_frames--;
      if (reading_pause_frames <= 0) {
        break;
      }
    }

    /* Find current active line */
    int active_line = 0;
    for (int l = 0; l < line_count; l++) {
      if (chars_revealed >= lines[l].start) {
        active_line = l;
      }
    }

    /* Auto-scroll to follow cursor during typing */
    if (!is_typing_done) {
      if (active_line >= VISIBLE_LINES) {
        target_scroll_line = active_line - (VISIBLE_LINES - 1);
      } else {
        target_scroll_line = 0;
      }
    }

    /* Smooth scroll interpolation */
    float target_y = (float)(target_scroll_line * LINE_HEIGHT);
    scroll_y += (target_y - scroll_y) * 0.35f;
    if (scroll_y < 0.0f) scroll_y = 0.0f;

    /* Render OLED frame */
    oled_clear_buffer();
    oled_set_draw_color(1);

    /* Render visible quote lines */
    for (int l = 0; l < line_count; l++) {
      int line_base_y = BASE_Y + (l * LINE_HEIGHT) - (int)scroll_y;
      if (line_base_y < 12 || line_base_y > 66) {
        continue;
      }

      if (chars_revealed <= lines[l].start) {
        continue;
      }

      char line_buf[64];
      int visible_len = chars_revealed - lines[l].start;
      if (visible_len > lines[l].len) {
        visible_len = lines[l].len;
      }
      if (visible_len >= sizeof(line_buf)) {
        visible_len = sizeof(line_buf) - 1;
      }

      memcpy(line_buf, &quote_str[lines[l].start], visible_len);
      line_buf[visible_len] = '\0';
      oled_draw_str(2, line_base_y, line_buf);

      /* Typing cursor on active line */
      if (l == active_line && !is_typing_done) {
        if ((frame / 4) % 2 == 0) {
          int cur_x = 2 + oled_get_str_width(line_buf) + 1;
          int cur_y = line_base_y - 9;
          if (cur_x < 125 && cur_y >= 12) {
            oled_draw_box(cur_x, cur_y, 2, 10);
          }
        }
      }
    }

    /* Clear and redraw header bar to ensure clean clipping */
    oled_set_draw_color(0);
    oled_draw_box(0, 0, 128, 12);
    oled_set_draw_color(1);

    oled_draw_str(2, 9, "~ BABEL ~");
    oled_draw_str(126 - badge_w, 9, badge);
    oled_draw_H_dotted_line(0, 11, 128);

    /* Scroll position indicator bar on right edge */
    if (line_count > VISIBLE_LINES) {
      int track_h = 48;
      int thumb_h = (VISIBLE_LINES * track_h) / line_count;
      if (thumb_h < 4) thumb_h = 4;
      int max_scroll = (line_count - VISIBLE_LINES) * LINE_HEIGHT;
      int thumb_y = 13;
      if (max_scroll > 0) {
        thumb_y += (int)((scroll_y * (track_h - thumb_h)) / max_scroll);
      }
      oled_draw_V_dotted_line(127, 13, track_h);
      oled_draw_V_line(127, thumb_y, thumb_h);
    }

    oled_send_buffer();
    frame++;
    vTaskDelay(pdMS_TO_TICKS(33));
  }

  /* Smooth exit wipe transition */
  for (int step = 0; step < 8; step++) {
    oled_set_draw_color(0);
    for (int y = step; y < 64; y += 8) {
      oled_draw_H_line(0, y, 128);
    }
    oled_send_buffer();
    vTaskDelay(pdMS_TO_TICKS(18));
  }

  oled_clear();
  ESP_LOGI(TAG, "Boot quote intro finished");
}
