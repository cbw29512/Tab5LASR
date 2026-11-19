#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Actual JPEG only, from device camera; size and CRC prevent fake/partial
// frames appearing as live video. The output is one ASCII LF-terminated line.
// Returns 0 for buffer-too-small or invalid arguments.
size_t lasr_usb_frame_format(const uint8_t *jpeg, size_t jpg_bytes,
                             uint32_t sequence, uint32_t width, uint32_t height,
                             char *destination, size_t destination_capacity);
