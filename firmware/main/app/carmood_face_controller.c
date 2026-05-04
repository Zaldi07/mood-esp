#include "app/carmood_face_controller.h"

#include "ui/carmood_ui.h"

#define CARMOOD_SEQ_MAX_STEPS 4

typedef struct {
    carmood_expr_t base_expr;
    uint16_t action_min_ms;
    uint16_t action_jitter_ms;
} carmood_persona_profile_t;

static uint32_t carmood_next_random(carmood_face_controller_t *face_state)
{
    face_state->rng_state = face_state->rng_state * 1664525U + 1013904223U;
    return face_state->rng_state;
}

static carmood_persona_profile_t carmood_get_persona_profile(int persona)
{
    switch ((carmood_pet_persona_t)persona) {
        case CARMOOD_PET_PERSONA_PLAYFUL:
            return (carmood_persona_profile_t){
                .base_expr = EXPR_HAPPY,
                .action_min_ms = 2200,
                .action_jitter_ms = 1600,
            };
        case CARMOOD_PET_PERSONA_SLEEPY:
            return (carmood_persona_profile_t){
                .base_expr = EXPR_SLEEPY,
                .action_min_ms = 4200,
                .action_jitter_ms = 2600,
            };
        case CARMOOD_PET_PERSONA_TSUNDERE:
            return (carmood_persona_profile_t){
                .base_expr = EXPR_ANGRY,
                .action_min_ms = 2500,
                .action_jitter_ms = 2000,
            };
        case CARMOOD_PET_PERSONA_CURIOUS:
            return (carmood_persona_profile_t){
                .base_expr = EXPR_THINKING,
                .action_min_ms = 2400,
                .action_jitter_ms = 1800,
            };
        case CARMOOD_PET_PERSONA_COOL:
            return (carmood_persona_profile_t){
                .base_expr = EXPR_COOL,
                .action_min_ms = 5200,
                .action_jitter_ms = 3200,
            };
        case CARMOOD_PET_PERSONA_DEFAULT:
        default:
            return (carmood_persona_profile_t){
                .base_expr = EXPR_IDLE,
                .action_min_ms = 2600,
                .action_jitter_ms = 2200,
            };
    }
}

static void carmood_schedule_next_action(carmood_face_controller_t *face_state,
                                         int64_t now_ms)
{
    carmood_persona_profile_t profile =
        carmood_get_persona_profile(face_state->persona);
    uint32_t jitter = profile.action_jitter_ms == 0 ? 0U :
                      carmood_next_random(face_state) % profile.action_jitter_ms;

    face_state->base_started_ms = now_ms;
    face_state->next_action_ms = now_ms + profile.action_min_ms + (int64_t)jitter;
}

static void carmood_cancel_sequence(carmood_face_controller_t *face_state)
{
    face_state->sequence_len = 0;
    face_state->sequence_index = 0;
    face_state->sequence_step_until_ms = -1;
    face_state->interaction_until_ms = -1;
}

static void carmood_apply_face_expr(carmood_face_controller_t *face_state,
                                    carmood_expr_t expr)
{
    if (face_state->active_expr == expr) {
        return;
    }
    face_state->active_expr = expr;
    carmood_ui_set_expression(expr);
}

static void carmood_restore_base_face(carmood_face_controller_t *face_state,
                                      int64_t now_ms)
{
    carmood_persona_profile_t profile =
        carmood_get_persona_profile(face_state->persona);

    carmood_cancel_sequence(face_state);
    face_state->base_expr = profile.base_expr;
    carmood_schedule_next_action(face_state, now_ms);
    carmood_apply_face_expr(face_state, face_state->base_expr);
}

static void carmood_start_sequence(carmood_face_controller_t *face_state,
                                   int64_t now_ms,
                                   const carmood_expr_t *exprs,
                                   const uint16_t *durations_ms,
                                   uint8_t len)
{
    if (len == 0 || len > CARMOOD_SEQ_MAX_STEPS) {
        carmood_cancel_sequence(face_state);
        return;
    }

    face_state->sequence_len = len;
    face_state->sequence_index = 0;
    for (uint8_t i = 0; i < len; ++i) {
        face_state->sequence_exprs[i] = exprs[i];
        face_state->sequence_durations_ms[i] = durations_ms[i];
    }

    carmood_apply_face_expr(face_state, face_state->sequence_exprs[0]);
    face_state->sequence_step_until_ms = now_ms + face_state->sequence_durations_ms[0];
    face_state->interaction_until_ms = -1;
}

static void carmood_start_single_step(carmood_face_controller_t *face_state,
                                      int64_t now_ms,
                                      carmood_expr_t expr,
                                      uint16_t duration_ms)
{
    const carmood_expr_t exprs[] = {expr};
    const uint16_t durations[] = {duration_ms};

    carmood_start_sequence(face_state, now_ms, exprs, durations, 1);
}

static void carmood_start_double_step(carmood_face_controller_t *face_state,
                                      int64_t now_ms,
                                      carmood_expr_t expr1,
                                      uint16_t duration1_ms,
                                      carmood_expr_t expr2,
                                      uint16_t duration2_ms)
{
    const carmood_expr_t exprs[] = {expr1, expr2};
    const uint16_t durations[] = {duration1_ms, duration2_ms};

    carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
}

static bool idle_seq_blink(carmood_face_controller_t *face_state, int64_t now_ms)
{
    carmood_start_single_step(face_state, now_ms, EXPR_BLINK, 220);
    return true;
}

static bool carmood_idle_action_default(carmood_face_controller_t *face_state,
                                        int64_t now_ms)
{
    uint32_t roll = carmood_next_random(face_state) % 14U;

    if (roll < 3U) {
        return idle_seq_blink(face_state, now_ms);
    } else if (roll < 5U) {
        carmood_start_single_step(face_state, now_ms, EXPR_SURPRISED, 1200);
        return true;
    } else if (roll < 7U) {
        carmood_start_single_step(face_state, now_ms, EXPR_HAPPY, 1500);
        return true;
    } else if (roll < 9U) {
        carmood_start_single_step(face_state, now_ms, EXPR_WINK, 1100);
        return true;
    } else if (roll < 11U) {
        carmood_start_single_step(face_state, now_ms, EXPR_THINKING, 1500);
        return true;
    } else if (roll < 13U) {
        carmood_start_single_step(face_state, now_ms, EXPR_SLEEPY, 1800);
        return true;
    } else {
        return false;
    }
}

static bool carmood_idle_action_playful(carmood_face_controller_t *face_state,
                                        int64_t now_ms)
{
    uint32_t roll = carmood_next_random(face_state) % 18U;

    if (roll < 3U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_HAPPY, 1400,
                                  EXPR_WINK, 700);
        return true;
    } else if (roll < 6U) {
        carmood_start_single_step(face_state, now_ms, EXPR_EXCITED, 1500);
        return true;
    } else if (roll < 7U) {
        carmood_start_single_step(face_state, now_ms, EXPR_LOVE, 1400);
        return true;
    } else if (roll < 9U) {
        carmood_start_single_step(face_state, now_ms, EXPR_WINK, 900);
        return true;
    } else if (roll < 11U) {
        return idle_seq_blink(face_state, now_ms);
    } else if (roll < 13U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_SURPRISED, 1000,
                                  EXPR_EXCITED, 1200);
        return true;
    } else if (roll < 15U) {
        carmood_start_single_step(face_state, now_ms, EXPR_DIZZY, 1000);
        return true;
    } else {
        return false;
    }
}

static bool carmood_idle_action_sleepy(carmood_face_controller_t *face_state,
                                       int64_t now_ms)
{
    uint32_t roll = carmood_next_random(face_state) % 14U;

    if (roll < 4U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_SLEEPY, 1800,
                                  EXPR_BLINK, 260);
        return true;
    } else if (roll < 7U) {
        carmood_start_single_step(face_state, now_ms, EXPR_YAWN, 1800);
        return true;
    } else if (roll < 9U) {
        return idle_seq_blink(face_state, now_ms);
    } else if (roll < 11U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_BLINK, 220,
                                  EXPR_YAWN, 1600);
        return true;
    } else {
        return false;
    }
}

static bool carmood_idle_action_tsundere(carmood_face_controller_t *face_state,
                                         int64_t now_ms)
{
    uint32_t roll = carmood_next_random(face_state) % 16U;

    if (roll < 3U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_ANGRY, 1200,
                                  EXPR_SHY, 1100);
        return true;
    } else if (roll < 5U) {
        carmood_start_single_step(face_state, now_ms, EXPR_SHY, 1300);
        return true;
    } else if (roll < 7U) {
        carmood_start_single_step(face_state, now_ms, EXPR_ANGRY, 1600);
        return true;
    } else if (roll < 9U) {
        return idle_seq_blink(face_state, now_ms);
    } else if (roll < 11U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_WINK, 900,
                                  EXPR_ANGRY, 1100);
        return true;
    } else if (roll < 13U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_SHY, 1100,
                                  EXPR_WINK, 900);
        return true;
    } else {
        return false;
    }
}

static bool carmood_idle_action_curious(carmood_face_controller_t *face_state,
                                        int64_t now_ms)
{
    uint32_t roll = carmood_next_random(face_state) % 16U;

    if (roll < 3U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_THINKING, 1500,
                                  EXPR_SURPRISED, 1000);
        return true;
    } else if (roll < 5U) {
        carmood_start_single_step(face_state, now_ms, EXPR_CONFUSED, 1200);
        return true;
    } else if (roll < 7U) {
        carmood_start_single_step(face_state, now_ms, EXPR_SURPRISED, 1100);
        return true;
    } else if (roll < 9U) {
        return idle_seq_blink(face_state, now_ms);
    } else if (roll < 11U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_CONFUSED, 1200,
                                  EXPR_THINKING, 1400);
        return true;
    } else if (roll < 13U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_THINKING, 1400,
                                  EXPR_WINK, 800);
        return true;
    } else {
        return false;
    }
}

static bool carmood_idle_action_cool(carmood_face_controller_t *face_state,
                                     int64_t now_ms)
{
    uint32_t roll = carmood_next_random(face_state) % 12U;

    if (roll < 3U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_COOL, 2200,
                                  EXPR_WINK, 1000);
        return true;
    } else if (roll < 4U) {
        carmood_start_single_step(face_state, now_ms, EXPR_WINK, 1100);
        return true;
    } else if (roll < 5U) {
        return idle_seq_blink(face_state, now_ms);
    } else if (roll < 6U) {
        carmood_start_double_step(face_state, now_ms,
                                  EXPR_COOL, 2400,
                                  EXPR_BLINK, 240);
        return true;
    } else {
        return false;
    }
}

static bool carmood_start_idle_action(carmood_face_controller_t *face_state,
                                      int64_t now_ms)
{
    switch (face_state->persona) {
        case CARMOOD_PET_PERSONA_PLAYFUL:
            return carmood_idle_action_playful(face_state, now_ms);
        case CARMOOD_PET_PERSONA_SLEEPY:
            return carmood_idle_action_sleepy(face_state, now_ms);
        case CARMOOD_PET_PERSONA_TSUNDERE:
            return carmood_idle_action_tsundere(face_state, now_ms);
        case CARMOOD_PET_PERSONA_CURIOUS:
            return carmood_idle_action_curious(face_state, now_ms);
        case CARMOOD_PET_PERSONA_COOL:
            return carmood_idle_action_cool(face_state, now_ms);
        default:
            return carmood_idle_action_default(face_state, now_ms);
    }
}

static void carmood_trigger_default_interaction(carmood_face_controller_t *face_state,
                                                int64_t now_ms)
{
    switch (carmood_next_random(face_state) % 4U) {
        case 0:
        {
            const carmood_expr_t exprs[] = {EXPR_SURPRISED, EXPR_HAPPY};
            const uint16_t durations[] = {420, 920};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 1:
        {
            const carmood_expr_t exprs[] = {EXPR_HAPPY, EXPR_BLINK};
            const uint16_t durations[] = {720, 260};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 2:
        {
            const carmood_expr_t exprs[] = {EXPR_WINK, EXPR_HAPPY};
            const uint16_t durations[] = {520, 700};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        default:
        {
            const carmood_expr_t exprs[] = {EXPR_THINKING, EXPR_BLINK};
            const uint16_t durations[] = {780, 240};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
    }
}

static void carmood_trigger_playful_interaction(carmood_face_controller_t *face_state,
                                                int64_t now_ms)
{
    switch (carmood_next_random(face_state) % 4U) {
        case 0:
        {
            const carmood_expr_t exprs[] = {EXPR_EXCITED, EXPR_HAPPY};
            const uint16_t durations[] = {700, 820};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 1:
        {
            const carmood_expr_t exprs[] = {EXPR_LOVE, EXPR_EXCITED};
            const uint16_t durations[] = {820, 760};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 2:
        {
            const carmood_expr_t exprs[] = {EXPR_WINK, EXPR_HAPPY};
            const uint16_t durations[] = {500, 640};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        default:
        {
            const carmood_expr_t exprs[] = {EXPR_SURPRISED, EXPR_EXCITED};
            const uint16_t durations[] = {320, 900};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
    }
}

static void carmood_trigger_sleepy_interaction(carmood_face_controller_t *face_state,
                                               int64_t now_ms)
{
    switch (carmood_next_random(face_state) % 4U) {
        case 0:
        {
            const carmood_expr_t exprs[] = {EXPR_YAWN, EXPR_SLEEPY};
            const uint16_t durations[] = {1100, 900};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 1:
        {
            const carmood_expr_t exprs[] = {EXPR_SLEEPY, EXPR_BLINK, EXPR_SLEEPY};
            const uint16_t durations[] = {900, 240, 820};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 3);
            return;
        }
        case 2:
        {
            const carmood_expr_t exprs[] = {EXPR_BLINK, EXPR_YAWN};
            const uint16_t durations[] = {220, 980};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        default:
        {
            const carmood_expr_t exprs[] = {EXPR_SLEEPY};
            const uint16_t durations[] = {1200};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 1);
            return;
        }
    }
}

static void carmood_trigger_tsundere_interaction(carmood_face_controller_t *face_state,
                                                 int64_t now_ms)
{
    switch (carmood_next_random(face_state) % 4U) {
        case 0:
        {
            const carmood_expr_t exprs[] = {EXPR_ANGRY, EXPR_SHY};
            const uint16_t durations[] = {720, 680};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 1:
        {
            const carmood_expr_t exprs[] = {EXPR_SHY, EXPR_WINK};
            const uint16_t durations[] = {620, 420};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 2:
        {
            const carmood_expr_t exprs[] = {EXPR_ANGRY, EXPR_BLINK};
            const uint16_t durations[] = {900, 240};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        default:
        {
            const carmood_expr_t exprs[] = {EXPR_SHY};
            const uint16_t durations[] = {900};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 1);
            return;
        }
    }
}

static void carmood_trigger_curious_interaction(carmood_face_controller_t *face_state,
                                                int64_t now_ms)
{
    switch (carmood_next_random(face_state) % 4U) {
        case 0:
        {
            const carmood_expr_t exprs[] = {EXPR_THINKING, EXPR_SURPRISED};
            const uint16_t durations[] = {860, 720};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 1:
        {
            const carmood_expr_t exprs[] = {EXPR_CONFUSED, EXPR_THINKING};
            const uint16_t durations[] = {900, 760};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 2:
        {
            const carmood_expr_t exprs[] = {EXPR_SURPRISED, EXPR_CONFUSED};
            const uint16_t durations[] = {520, 760};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        default:
        {
            const carmood_expr_t exprs[] = {EXPR_THINKING};
            const uint16_t durations[] = {1200};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 1);
            return;
        }
    }
}

static void carmood_trigger_cool_interaction(carmood_face_controller_t *face_state,
                                             int64_t now_ms)
{
    switch (carmood_next_random(face_state) % 4U) {
        case 0:
        {
            const carmood_expr_t exprs[] = {EXPR_COOL, EXPR_WINK};
            const uint16_t durations[] = {1100, 520};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 1:
        {
            const carmood_expr_t exprs[] = {EXPR_COOL, EXPR_BLINK};
            const uint16_t durations[] = {920, 240};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        case 2:
        {
            const carmood_expr_t exprs[] = {EXPR_WINK, EXPR_COOL};
            const uint16_t durations[] = {500, 940};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 2);
            return;
        }
        default:
        {
            const carmood_expr_t exprs[] = {EXPR_COOL};
            const uint16_t durations[] = {1300};
            carmood_start_sequence(face_state, now_ms, exprs, durations, 1);
            return;
        }
    }
}

void carmood_face_init(carmood_face_controller_t *face_state,
                       int64_t now_ms,
                       uint32_t rng_seed)
{
    face_state->active_expr = EXPR_IDLE;
    face_state->base_expr = EXPR_IDLE;
    face_state->base_started_ms = now_ms;
    face_state->next_action_ms = -1;
    face_state->interaction_until_ms = -1;
    face_state->sequence_step_until_ms = -1;
    face_state->rng_state = rng_seed;
    face_state->persona = CARMOOD_PET_PERSONA_DEFAULT;
    face_state->sequence_len = 0;
    face_state->sequence_index = 0;

    carmood_restore_base_face(face_state, now_ms);
}

void carmood_face_update(carmood_face_controller_t *face_state, int64_t now_ms)
{
    if (face_state->sequence_len != 0) {
        if (now_ms < face_state->sequence_step_until_ms) {
            return;
        }

        face_state->sequence_index++;
        if (face_state->sequence_index >= face_state->sequence_len) {
            carmood_restore_base_face(face_state, now_ms);
            return;
        }

        carmood_apply_face_expr(face_state,
                                face_state->sequence_exprs[face_state->sequence_index]);
        face_state->sequence_step_until_ms =
            now_ms + face_state->sequence_durations_ms[face_state->sequence_index];
        return;
    }

    if (now_ms < face_state->next_action_ms) {
        return;
    }

    if (!carmood_start_idle_action(face_state, now_ms)) {
        carmood_restore_base_face(face_state, now_ms);
    }
}

void carmood_face_restore_idle(carmood_face_controller_t *face_state, int64_t now_ms)
{
    carmood_restore_base_face(face_state, now_ms);
}

void carmood_face_set_persona(carmood_face_controller_t *face_state,
                              carmood_pet_persona_t persona,
                              int64_t now_ms)
{
    face_state->persona = (int)persona;
    carmood_face_restore_idle(face_state, now_ms);
}

void carmood_face_trigger_interaction(carmood_face_controller_t *face_state,
                                      int64_t now_ms)
{
    switch ((carmood_pet_persona_t)face_state->persona) {
        case CARMOOD_PET_PERSONA_PLAYFUL:
            carmood_trigger_playful_interaction(face_state, now_ms);
            return;
        case CARMOOD_PET_PERSONA_SLEEPY:
            carmood_trigger_sleepy_interaction(face_state, now_ms);
            return;
        case CARMOOD_PET_PERSONA_TSUNDERE:
            carmood_trigger_tsundere_interaction(face_state, now_ms);
            return;
        case CARMOOD_PET_PERSONA_CURIOUS:
            carmood_trigger_curious_interaction(face_state, now_ms);
            return;
        case CARMOOD_PET_PERSONA_COOL:
            carmood_trigger_cool_interaction(face_state, now_ms);
            return;
        case CARMOOD_PET_PERSONA_DEFAULT:
        default:
            carmood_trigger_default_interaction(face_state, now_ms);
            return;
    }
}

bool carmood_face_should_restore_idle(const carmood_face_controller_t *face_state,
                                      int64_t now_ms)
{
    return face_state->interaction_until_ms >= 0 &&
           now_ms >= face_state->interaction_until_ms;
}
