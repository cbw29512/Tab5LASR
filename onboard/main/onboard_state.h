#pragma once
#include <stdint.h>

typedef enum {
    ONBOARD_BOOT = 0,
    ONBOARD_CAMERA_INIT,
    ONBOARD_FORMAT_READY,
    ONBOARD_STREAMING,
    ONBOARD_ERROR
} onboard_camera_phase_t;

typedef struct {
    onboard_camera_phase_t state;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t captured_frames;
    uint32_t displayed_frames;
    uint32_t failed_frames;
    int64_t last_frame_monotonic_us;
} onboard_camera_status_t;
extern onboard_camera_status_t g_onboard_status;
