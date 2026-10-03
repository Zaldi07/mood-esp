#include "drivers/buzzer.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "buzzer";

#define BUZZER_LEDC_TIMER       LEDC_TIMER_0
#define BUZZER_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BUZZER_DUTY_RES         LEDC_TIMER_10_BIT
#define BUZZER_DUTY_50_PCT      512   /* (1 << 10) / 2 */
#define BUZZER_DEFAULT_FREQ     2700

static bool s_initialized = false;
static bool s_is_on = false;
static bool s_alarm_active = false;
static int64_t s_action_end_us = 0;
static int64_t s_alarm_start_us = 0;

esp_err_t buzzer_init(void)
{
    if (s_initialized) return ESP_OK;

    ledc_timer_config_t timer_conf = {
        .speed_mode       = BUZZER_LEDC_MODE,
        .duty_resolution  = BUZZER_DUTY_RES,
        .timer_num        = BUZZER_LEDC_TIMER,
        .freq_hz          = BUZZER_DEFAULT_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t ch_conf = {
        .gpio_num   = BUZZER_DEFAULT_PIN,
        .speed_mode = BUZZER_LEDC_MODE,
        .channel    = BUZZER_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = BUZZER_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ret = ledc_channel_config(&ch_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    s_is_on = false;
    s_alarm_active = false;
    ESP_LOGI(TAG, "Buzzer driver initialized on GPIO %d", BUZZER_DEFAULT_PIN);
    return ESP_OK;
}

void buzzer_tone_on(uint32_t freq_hz)
{
    if (!s_initialized) buzzer_init();
    if (freq_hz > 0) {
        ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, freq_hz);
    }
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, BUZZER_DUTY_50_PCT);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    s_is_on = true;
}

void buzzer_off(void)
{
    if (!s_initialized) return;
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    s_is_on = false;
}

void buzzer_beep(uint32_t duration_ms, uint32_t freq_hz)
{
    buzzer_tone_on(freq_hz > 0 ? freq_hz : BUZZER_DEFAULT_FREQ);
    s_action_end_us = esp_timer_get_time() + ((int64_t)duration_ms * 1000LL);
}

void buzzer_start_alarm(void)
{
    if (!s_initialized) buzzer_init();
    s_alarm_active = true;
    s_alarm_start_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Pomodoro alarm started!");
}

void buzzer_stop(void)
{
    s_alarm_active = false;
    s_action_end_us = 0;
    buzzer_off();
}

bool buzzer_is_alarm_active(void)
{
    return s_alarm_active;
}

void buzzer_tick(void)
{
    if (!s_initialized) return;

    int64_t now_us = esp_timer_get_time();

    /* Handle single beep timeout */
    if (!s_alarm_active && s_action_end_us > 0) {
        if (now_us >= s_action_end_us) {
            buzzer_off();
            s_action_end_us = 0;
        }
        return;
    }

    /* Handle Pomodoro rhythmic triple-beep alarm */
    if (s_alarm_active) {
        int64_t elapsed_ms = (now_us - s_alarm_start_us) / 1000LL;
        /* Rhythmic cycle length: 1300 ms total */
        uint32_t cycle_pos_ms = (uint32_t)(elapsed_ms % 1300LL);

        /* Beep 1: 0 - 100 ms (2700 Hz) */
        if (cycle_pos_ms < 100) {
            if (!s_is_on) buzzer_tone_on(2700);
        }
        /* Silence 1: 100 - 180 ms */
        else if (cycle_pos_ms < 180) {
            if (s_is_on) buzzer_off();
        }
        /* Beep 2: 180 - 280 ms (2700 Hz) */
        else if (cycle_pos_ms < 280) {
            if (!s_is_on) buzzer_tone_on(2700);
        }
        /* Silence 2: 280 - 360 ms */
        else if (cycle_pos_ms < 360) {
            if (s_is_on) buzzer_off();
        }
        /* Beep 3: 360 - 520 ms (3100 Hz high pitch) */
        else if (cycle_pos_ms < 520) {
            if (!s_is_on) buzzer_tone_on(3100);
        }
        /* Rest silence: 520 - 1300 ms */
        else {
            if (s_is_on) buzzer_off();
        }
    }
}

/* Space chiptune boot intro melody */
static const uint32_t s_intro_melody[] = {
    523, 659, 784, 1047,  /* NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6 */
    988, 784, 659, 523,   /* NOTE_B5, NOTE_G5, NOTE_E5, NOTE_C5 */
    587, 698, 880, 1047,  /* NOTE_D5, NOTE_F5, NOTE_A5, NOTE_C6 */
    988, 880, 784, 659    /* NOTE_B5, NOTE_A5, NOTE_G5, NOTE_E5 */
};

static const uint8_t s_intro_durations[] = {
    8, 8, 8, 4,
    8, 8, 8, 4,
    8, 8, 8, 4,
    8, 8, 8, 4
};

void buzzer_play_intro_melody(void)
{
    if (!s_initialized) buzzer_init();

    ESP_LOGI(TAG, "Playing chiptune intro melody on GPIO %d", BUZZER_DEFAULT_PIN);
    for (int i = 0; i < 16; i++) {
        uint32_t note_dur_ms = 1000 / s_intro_durations[i];
        buzzer_tone_on(s_intro_melody[i]);
        vTaskDelay(pdMS_TO_TICKS(note_dur_ms));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS((note_dur_ms * 30) / 100));
    }
}

static void intro_melody_task(void *pvParameters)
{
    buzzer_play_intro_melody();
    vTaskDelete(NULL);
}

void buzzer_play_intro_melody_async(void)
{
    xTaskCreate(intro_melody_task, "intro_melody", 2048, NULL, 5, NULL);
}

void buzzer_play_sleep_melody(void)
{
    if (!s_initialized) buzzer_init();
    const uint32_t notes[] = { 659, 523, 392, 261 };
    const uint32_t durs[]  = { 120, 150, 180, 320 };
    for (int i = 0; i < 4; i++) {
        buzzer_tone_on(notes[i]);
        vTaskDelay(pdMS_TO_TICKS(durs[i]));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(35));
    }
}


