#pragma once
#include "lasr_raw_pixel.h"
#include <stdbool.h>
#include <stdint.h>

// User-calibrated settings (NOT validated for SIRT until physical sampling).
typedef struct {
    float left, top, right, bottom; // normalized raw camera ROI
    uint8_t min_luma, min_delta;    // bright and changed relative to reference
    uint8_t step;                   // sample every 1..8 pixels
    uint8_t min_cluster;            // >= 1 locally clustered samples
    uint16_t radius_px, max_candidates;
} lasr_blob_config_t;
typedef struct {
    bool visible;
    float x_norm, y_norm;
    uint32_t candidates, cluster;
    uint8_t peak_delta;
} lasr_blob_result_t;

// Reference must be separately owned and captured from a CONFIRMED dark target.
// No camera buffer may be retained across V4L2 QBUF.
bool lasr_blob_scan(const lasr_raw_frame_t *current,
                    const lasr_raw_frame_t *reference,
                    const lasr_blob_config_t *config, lasr_blob_result_t *out);