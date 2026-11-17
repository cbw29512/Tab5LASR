#pragma once
#include <stdint.h>

// Pure/stateless math: does not include ESP-IDF; tested with host C compiler.
double camera_rate_fps(uint32_t frames_delta, int64_t elapsed_microseconds);
