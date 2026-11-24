#pragma once
#include "lasr_laser_blob.h"
#include "lasr_laser_pulse.h"
#include <stdbool.h>
#include <stdint.h>

// The pure pipeline supplies source coordinates and time; no fake score/shot.
typedef struct {
    lasr_blob_config_t blob;
    lasr_pulse_detector_t pulse;
    uint32_t scanned_frames, bright_frames;
} lasr_laser_pipeline_t;

bool lasr_laser_pipeline_init(lasr_laser_pipeline_t *pipeline,
                              lasr_blob_config_t blob, lasr_pulse_config_t pulse);
bool lasr_laser_pipeline_step(lasr_laser_pipeline_t *pipeline,
                              const lasr_raw_frame_t *now,
                              const lasr_raw_frame_t *dark,
                              uint64_t capture_us, lasr_pulse_event_t *out);