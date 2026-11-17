#include <assert.h>
#include <math.h>
#include "../main/camera_metrics.h"

int main(void) {
    assert(camera_rate_fps(30, 1000000) == 30.0);
    assert(camera_rate_fps(0, 1000000) == 0.0);
    assert(camera_rate_fps(150, 5000000) == 30.0);
    assert(camera_rate_fps(500, 0) == 0.0);
    assert(camera_rate_fps(500, -1) == 0.0);
    assert(fabs(camera_rate_fps(1, 33333) - 30.0003) < 0.01);
    return 0;
}
