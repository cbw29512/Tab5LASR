#pragma once
#include "lasr_raw_pixel.h"
#include <stdbool.h>
#include <stdint.h>

// Map the negotiated V4L2 fourcc, never an assumed camera sensor mode.
bool lasr_csi_format_map(uint32_t fourcc, lasr_raw_format_t *out);