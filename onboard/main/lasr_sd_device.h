#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Only the dedicated storage owner task may invoke these functions.
// The module deliberately has NO automatic mount at boot.
esp_err_t lasr_sd_mount_noformat(void);
esp_err_t lasr_sd_unmount(void);
bool lasr_sd_is_mounted(void);
const char *lasr_sd_root(void);