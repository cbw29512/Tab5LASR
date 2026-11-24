#pragma once
#include <stdbool.h>
#include <stdint.h>

// Only a threshold-calibrated raw CSI scanner may supply visibility/coordinates.
// Parameters must be determined by a physical SIRT test, not a cosmetic UI mode.
typedef struct {
    uint8_t on_frames, off_frames;
    uint64_t max_gap_us;
} lasr_pulse_config_t;

typedef enum { LASR_PULSE_WAIT_DARK, LASR_PULSE_ARMED,
               LASR_PULSE_CANDIDATE, LASR_PULSE_HELD,
               LASR_PULSE_REARMING } lasr_pulse_phase_t;

typedef struct {
    bool shot_detected;
    uint64_t first_seen_us;
    float x_norm, y_norm;
} lasr_pulse_event_t;

typedef struct {
    lasr_pulse_config_t config;
    lasr_pulse_phase_t phase;
    uint8_t dark_count, visible_count;
    uint64_t last_frame_us, first_visible_us;
    float first_x, first_y;
} lasr_pulse_detector_t;

bool lasr_pulse_init(lasr_pulse_detector_t *d, lasr_pulse_config_t config);
bool lasr_pulse_step(lasr_pulse_detector_t *d, uint64_t frame_us,
                     bool visible, float x_norm, float y_norm,
                     lasr_pulse_event_t *out);