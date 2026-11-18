#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LASR_SESSION_PATH_MAX 256
#define LASR_TARGET_MAX 32

typedef enum {
    LASR_STORE_EMPTY = 0, LASR_STORE_RECORDING, LASR_STORE_SAVED,
    LASR_STORE_INCOMPLETE, LASR_STORE_ERROR
} lasr_store_phase_t;

typedef struct {
    uint32_t sequence;
    uint64_t elapsed_ms;   // Monotonic time since GO (pauses excluded)
    float x_norm, y_norm;  // Already calibrated camera-frame coordinates
    int32_t score;
    char zone[16];
} lasr_shot_t;

typedef struct {
    char directory[LASR_SESSION_PATH_MAX];
    char target[LASR_TARGET_MAX];
    char last_error[128];
    uint32_t calibration_revision;
    uint32_t delay_ms;
    uint32_t shot_count;
    uint64_t last_elapsed_ms;
    int64_t score_total;
    FILE *journal;
    lasr_store_phase_t phase;
} lasr_session_t;

// All entry points are called ONLY by the storage worker, not the camera ISR.
// A failed save is never silently upgraded to LASR_STORE_SAVED.
bool lasr_store_begin(lasr_session_t *session, const char *mount_path,
                      const char *target_id, uint32_t calibration_revision,
                      uint32_t delay_ms);
bool lasr_store_shot(lasr_session_t *session, const lasr_shot_t *shot);
bool lasr_store_finish(lasr_session_t *session, uint64_t final_elapsed_ms);
void lasr_store_abort(lasr_session_t *session);
// Checks the status marker and the finalized files, never just the filename.
bool lasr_store_verify_saved(const char *session_dir, uint32_t expected_shots);