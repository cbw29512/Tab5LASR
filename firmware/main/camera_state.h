#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_lcd_types.h"

#define LASR_CAM_W 320
#define LASR_CAM_H 240
#define LASR_CAM_FPS 15
#define LASR_MAX_JPEG_BYTES (64u * 1024u)

typedef enum {
    CAMERA_DISCONNECTED = 0,
    CAMERA_WAITING,
    CAMERA_NEGOTIATING,
    CAMERA_STREAMING,
    CAMERA_ERROR
} camera_phase_t;

typedef struct {
    uint8_t *bytes;             // Owns a JPEG copy; never a borrowed UVC pointer.
    size_t length;
    int64_t captured_us;
} camera_frame_t;

typedef struct {
    volatile uint32_t captured;
    volatile uint32_t dropped;
    volatile uint32_t decoded;
    volatile uint32_t displayed;
    volatile uint32_t decode_errors;
    volatile camera_phase_t phase;
} camera_stats_t;

extern camera_stats_t g_camera_stats;
bool camera_uvc_start(esp_lcd_panel_handle_t panel);
