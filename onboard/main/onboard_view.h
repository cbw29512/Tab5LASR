#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Owns persistent PSRAM display buffers; never displays a requeued V4L2 pointer.
bool onboard_view_init(uint32_t width, uint32_t height, uint32_t pixel_format);
bool onboard_view_show(const void *frame_bytes, size_t length, uint8_t buffer_index);
