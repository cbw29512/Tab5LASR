#pragma once
#include <stdbool.h>

// HTTP server must start only after the Wi-Fi transport is usable.
bool lasr_web_http_start(void);
