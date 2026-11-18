#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Producer uses the existing PPA RGB565 orientation, never a V4L2 borrow.
// This interface is disabled entirely in the known-good camera build.
bool lasr_web_frame_init(uint32_t width, uint32_t height);
void lasr_web_frame_offer(const uint16_t *pixels, uint32_t width,
                          uint32_t height);
size_t lasr_web_frame_copy(uint8_t *destination, size_t capacity,
                           uint32_t *sequence, int64_t *age_us);
uint32_t lasr_web_frame_count(void);
// Real JPEG age, distinct from the raw CSI sensor's latest-frame age.
// Returns -1 if encoder inactive or no successful JPEG exists.
int64_t lasr_web_frame_age_us(void);
