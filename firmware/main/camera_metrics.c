#include "camera_metrics.h"

double camera_rate_fps(uint32_t frames_delta, int64_t elapsed_microseconds) {
    if (elapsed_microseconds <= 0) return 0.0;
    return ((double)frames_delta * 1000000.0) / (double)elapsed_microseconds;
}
