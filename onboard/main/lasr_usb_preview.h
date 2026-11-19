#pragma once
#include <stdbool.h>
#include <stdint.h>

// Separate USB engineering build; never uses V4L2-owned frame pointers.
// Frame dimensions are fixed at 320x176 for bounded JPEG/serial transfer.
bool lasr_usb_preview_start(void);
void lasr_usb_preview_offer(const uint16_t *display_rgb565,
                            uint32_t display_width, uint32_t display_height);
