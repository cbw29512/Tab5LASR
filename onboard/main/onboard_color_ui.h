#pragma once
#include <stdbool.h>
#include "lvgl.h"
#include "onboard_color.h"

// All calls occur under the same LVGL lock as camera canvas updates.
// The color button changes only the converted framebuffer display mode.
bool onboard_color_ui_attach(lv_obj_t *screen);
onboard_color_mode_t onboard_color_ui_mode(void);
