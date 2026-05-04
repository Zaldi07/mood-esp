#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/carmood_ui.h"

typedef enum {
    CARMOOD_PET_PERSONA_DEFAULT = 0,
    CARMOOD_PET_PERSONA_PLAYFUL,
    CARMOOD_PET_PERSONA_SLEEPY,
    CARMOOD_PET_PERSONA_TSUNDERE,
    CARMOOD_PET_PERSONA_CURIOUS,
    CARMOOD_PET_PERSONA_COOL,
} carmood_pet_persona_t;

typedef struct {
    carmood_expr_t active_expr;
    carmood_expr_t base_expr;
    int64_t base_started_ms;
    int64_t next_action_ms;
    int64_t interaction_until_ms;
    int64_t sequence_step_until_ms;
    uint32_t rng_state;
    int persona;
    uint8_t sequence_len;
    uint8_t sequence_index;
    carmood_expr_t sequence_exprs[4];
    uint16_t sequence_durations_ms[4];
} carmood_face_controller_t;

void carmood_face_init(carmood_face_controller_t *face_state,
                       int64_t now_ms,
                       uint32_t rng_seed);
void carmood_face_update(carmood_face_controller_t *face_state, int64_t now_ms);
void carmood_face_restore_idle(carmood_face_controller_t *face_state, int64_t now_ms);
void carmood_face_set_persona(carmood_face_controller_t *face_state,
                              carmood_pet_persona_t persona,
                              int64_t now_ms);
void carmood_face_trigger_interaction(carmood_face_controller_t *face_state,
                                      int64_t now_ms);
bool carmood_face_should_restore_idle(const carmood_face_controller_t *face_state,
                                      int64_t now_ms);
