#pragma once
#include <stdbool.h>

// Starts read-only battery bus telemetry. Camera operation may continue on USB
// even if the battery cannot be verified. No automatic charger recovery.
bool tab5_battery_monitor_start(void);
