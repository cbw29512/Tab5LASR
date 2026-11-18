#pragma once
#include "lasr_session_store.h"
#include <stdbool.h>
#include <stdint.h>

// Schema first: only the SD worker may mutate this state machine.
typedef enum {
    LASR_REC_IDLE, LASR_REC_STARTING, LASR_REC_RECORDING,
    LASR_REC_FINALIZING, LASR_REC_SAVED, LASR_REC_UNSAVED
} lasr_rec_state_t;

typedef enum { LASR_REC_BEGIN, LASR_REC_SHOT, LASR_REC_FINISH } lasr_rec_command_t;

typedef struct {
    lasr_rec_command_t type;
    union {
        struct {
            char session_id[48], target_id[48];
            uint32_t calibration_revision, delay_ms;
        } begin;
        lasr_shot_t shot;
    } payload;
} lasr_rec_message_t;

typedef struct {
    lasr_rec_state_t state;
    lasr_session_store_t store;
    uint32_t discarded_events;
} lasr_recorder_core_t;

void lasr_recorder_core_init(lasr_recorder_core_t *core);
bool lasr_recorder_core_apply(lasr_recorder_core_t *core,
                             const lasr_rec_message_t *message, const char *root);
void lasr_recorder_core_lost_events(lasr_recorder_core_t *core, uint32_t lost);