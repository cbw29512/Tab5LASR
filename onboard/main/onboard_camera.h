#pragma once
#include <stdbool.h>

// Initializes CSI capture and runs the physical camera in the caller task.
// Returns only when initialization or streaming fails.
bool onboard_camera_run(void);
