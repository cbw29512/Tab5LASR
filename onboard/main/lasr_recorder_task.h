#pragma once
#include "lasr_recorder_core.h"
#include <stdbool.h>
#include <stdint.h>

// Explicit optional initialization, never called at boot in this review.
// Public submissions are task-context only; do not invoke from an ISR.
bool lasr_recorder_task_init(void);
bool lasr_recorder_task_begin(const char *session_id, const char *target_id,
                              uint32_t calibration_revision, uint32_t delay_ms);
bool lasr_recorder_task_shot(const lasr_shot_t *shot);
bool lasr_recorder_task_finish(void);
lasr_rec_state_t lasr_recorder_task_status(void);
uint32_t lasr_recorder_task_dropped(void);