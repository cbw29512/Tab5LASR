#pragma once
#include <stdbool.h>

// Engineering-only camera+microSD concurrency probe.
// Called after actual CSI frames are flowing, never from an ISR.
// Performs the proven no-format synthetic SD session test in another task.
bool lasr_concurrent_sd_start(void);
// Capture task calls after successful VIDIOC_QBUF (combined builds only).
void lasr_concurrent_sd_frame_returned(void);
