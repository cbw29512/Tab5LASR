#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// v1: one writer; caller serializes operations in an SD owner task.
typedef enum { LASR_STORE_IDLE, LASR_STORE_RECORDING, LASR_STORE_FINALIZING,
               LASR_STORE_SAVED, LASR_STORE_ERROR } lasr_store_state_t;
typedef struct {
    uint32_t sequence, elapsed_ms, split_ms;
    int32_t score;
    float x_norm, y_norm;
    char zone[24];
} lasr_shot_t;
typedef struct {
    lasr_store_state_t state;
    char folder[256], session_id[48], target_id[48];
    uint32_t calibration_revision, delay_ms, shots, last_elapsed_ms;
    int64_t total_score;
    FILE *journal;
    int last_errno;
} lasr_session_store_t;

bool lasr_store_begin(lasr_session_store_t *s, const char *root,
                      const char *session_id, const char *target_id,
                      uint32_t calibration_revision, uint32_t delay_ms);
bool lasr_store_append(lasr_session_store_t *s, const lasr_shot_t *shot);
bool lasr_store_finish(lasr_session_store_t *s);
void lasr_store_abort(lasr_session_store_t *s);