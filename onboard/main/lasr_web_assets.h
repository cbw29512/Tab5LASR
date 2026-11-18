#pragma once
#include "esp_http_server.h"

// Static browser assets are embedded into firmware at link time.
// No external cloud or filesystem dependencies.
bool lasr_web_assets_register(httpd_handle_t server);
