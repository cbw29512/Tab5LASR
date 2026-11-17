#include <assert.h>
#include "../main/onboard_stats.h"
int main(void) {
    assert(onboard_frame_rate(0, 1000000) == 0.0);
    assert(onboard_frame_rate(150, 5000000) == 30.0);
    assert(onboard_frame_rate(0, 0) == 0.0);
    assert(onboard_frame_rate(100, -10) == 0.0);
    return 0;
}
