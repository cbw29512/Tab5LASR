#pragma once
#include <stdbool.h>

// Test-only diagnostic report, separate from the live camera preview.
// Battery/recovery widgets remain attached to the main LVGL screen.
void lasr_sd_status_ui_show(bool success);