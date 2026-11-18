#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Diagnostic only: the camera sensor, PPA rotation, and battery state
// remain untouched while the user compares color representations.
typedef enum {
    ONBOARD_COLOR_ORIGINAL = 0,
    ONBOARD_COLOR_SWAP_RED_BLUE = 1,
    ONBOARD_COLOR_SWAP_BYTES = 2,
    ONBOARD_COLOR_SWAP_BOTH = 3,
    ONBOARD_COLOR_MODE_COUNT = 4
} onboard_color_mode_t;

// Pure pixel transform operating on the converted RGB565 canvas buffer.
// Original mode is zero-copy/no-op; other modes incur CPU work temporarily.
// Returns false on invalid arguments. All pixels remain within the buffer.
bool onboard_color_apply_rgb565(uint16_t *pixels, size_t pixel_count,
                               onboard_color_mode_t mode);

const char *onboard_color_mode_name(onboard_color_mode_t mode);
