#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Data schema first: V4L2 input, never display-corrected Color 2/4 bytes.
typedef enum {
    LASR_RAW_RGB565_LE, LASR_RAW_RGB565_BE, LASR_RAW_UYVY
} lasr_raw_format_t;
typedef struct {
    const uint8_t *data;
    size_t length, stride;
    uint32_t width, height;
    lasr_raw_format_t format;
} lasr_raw_frame_t;

// Every pixel consumes two bytes. Reject inaccessible rows rather than read
// a possibly unowned V4L2 buffer beyond bytesused.
bool lasr_raw_frame_valid(const lasr_raw_frame_t *frame);
bool lasr_raw_luma(const lasr_raw_frame_t *frame,
                   uint32_t x, uint32_t y, uint8_t *out);