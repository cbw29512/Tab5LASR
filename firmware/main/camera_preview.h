#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_lcd_types.h"

// Display is initialized once; only the frame worker calls draw.
bool camera_preview_open(esp_lcd_panel_handle_t *panel_out);
bool camera_preview_draw(esp_lcd_panel_handle_t panel,
                         const uint8_t *jpeg, size_t jpeg_bytes);
