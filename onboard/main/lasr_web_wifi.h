#pragma once
#include <stdbool.h>

// Starts the onboard C6 Wi-Fi in AP mode. No credentials are in source.
// Factory C6 firmware compatibility must be physically confirmed first.
bool lasr_web_wifi_start(void);
bool lasr_web_wifi_ready(void);
