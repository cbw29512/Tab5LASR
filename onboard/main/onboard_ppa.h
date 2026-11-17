#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// The P4 image accelerator turns MIPI-CSI pixel data into a fitted RGB565 canvas.
bool onboard_ppa_init(uint32_t width, uint32_t height,
                      uint32_t pixel_format, uint32_t *out_w, uint32_t *out_h);
bool onboard_ppa_convert(const void *input, void *output,
                         size_t output_bytes);
