#pragma once
#include <stdbool.h>
#include "lvgl.h"
// Attach only while owning bsp_display_lock; never cover charger recovery UI.
bool lasr_sd_widget_attach(lv_obj_t *screen);