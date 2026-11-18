#pragma once
#include "esp_err.h"

// Opt-in hardware test only. Never invoked in normal application builds.
esp_err_t lasr_sd_smoke_once(void);