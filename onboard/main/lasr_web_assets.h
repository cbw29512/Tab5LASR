#pragma once
#include <stdbool.h>
#include "esp_http_server.h"

// Embedded static assets require no filesystem or cloud dependency.
bool lasr_web_assets_register(httpd_handle_t server);
