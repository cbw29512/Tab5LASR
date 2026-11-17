#include "onboard_stats.h"
double onboard_frame_rate(uint32_t frame_count, int64_t elapsed_us) {
    if (elapsed_us <= 0) return 0.0;
    return (double)frame_count * 1000000.0 / (double)elapsed_us;
}
