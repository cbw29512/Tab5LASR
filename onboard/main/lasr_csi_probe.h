#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Diagnostic-only CSI hook. Never submits an uncalibrated shot to the recorder.
bool lasr_csi_probe_begin(uint32_t width, uint32_t height,
                          uint32_t v4l2_format, uint32_t stride);
bool lasr_csi_probe_observe(const void *borrowed, size_t length,
                            uint64_t timestamp_us);
void lasr_csi_probe_end(void);

// Combined engineering build: scanner starts OFF for baseline camera FPS.
// LVGL button toggles it while CSI runs; no SIRT events are saved.
void lasr_csi_probe_set_scanning_enabled(bool enabled);
bool lasr_csi_probe_scanning_enabled(void);
