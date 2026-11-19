#pragma once
#include "lasr_laser_pulse.h"
#include "lasr_session_store.h"
#include <stdbool.h>
#include <stdint.h>

// Actual device clock: timestamps are monotonic microseconds, never wall time.
typedef enum {
    LASR_CLOCK_IDLE, LASR_CLOCK_COUNTDOWN, LASR_CLOCK_ACTIVE,
    LASR_CLOCK_PAUSED, LASR_CLOCK_COMPLETE
} lasr_clock_phase_t;
typedef struct {
    lasr_clock_phase_t phase;
    uint32_t delay_ms, count;
    uint64_t countdown_end_us, go_us, accumulated_us;
    uint64_t last_event_us;
    uint32_t last_shot_elapsed_ms;
} lasr_shot_clock_t;

void lasr_clock_reset(lasr_shot_clock_t *clock);
bool lasr_clock_countdown(lasr_shot_clock_t *clock,
                          uint64_t start_us, uint32_t delay_ms);
bool lasr_clock_mark_go(lasr_shot_clock_t *clock, uint64_t actual_beep_us);
bool lasr_clock_pause(lasr_shot_clock_t *clock, uint64_t now_us);
bool lasr_clock_finish(lasr_shot_clock_t *clock, uint64_t now_us);
bool lasr_clock_unscored_hit(lasr_shot_clock_t *clock,
                            const lasr_pulse_event_t *pulse, lasr_shot_t *out);