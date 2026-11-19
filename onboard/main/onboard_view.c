#include "onboard_view.h"
#include "onboard_ppa.h"
#include "onboard_color.h"
#include "onboard_color_ui.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "lvgl.h"
#include "sdkconfig.h"
#if CONFIG_LASR_USB_PREVIEW_DIAGNOSTIC
#include "lasr_usb_preview.h"
#endif

static const char *TAG = "lasr_view";
static lv_obj_t *g_canvas;
static uint8_t *g_canvas_pixels[2];
static size_t g_capacity;
static uint32_t g_width, g_height;

bool onboard_view_init(uint32_t width, uint32_t height, uint32_t pixel_format) {
    if (!onboard_ppa_init(width, height, pixel_format, &g_width, &g_height)) {
        ESP_LOGE(TAG, "CSI format conversion setup failed");
        return false;
    }
    size_t alignment = 0;
    esp_err_t err = esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &alignment);
    if (err != ESP_OK || alignment == 0) {
        ESP_LOGE(TAG, "PSRAM cache alignment unavailable: %s",
                 esp_err_to_name(err));
        return false;
    }
    g_capacity = (size_t)BSP_LCD_H_RES * BSP_LCD_V_RES * 2;
    g_capacity = (g_capacity + alignment - 1) & ~(alignment - 1);
    for (unsigned i = 0; i < 2; i++) {
        g_canvas_pixels[i] = heap_caps_aligned_calloc(
            alignment, 1, g_capacity, MALLOC_CAP_SPIRAM);
        if (!g_canvas_pixels[i]) {
            ESP_LOGE(TAG, "Could not allocate aligned PSRAM preview buffer %u", i);
            return false;
        }
    }
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "LVGL display lock unavailable");
        return false;
    }
    g_canvas = lv_canvas_create(lv_scr_act());
    if (g_canvas) {
        // Keep the guarded battery recovery controls above the video canvas.
        lv_obj_move_background(g_canvas);
        lv_canvas_set_buffer(g_canvas, g_canvas_pixels[0],
                             g_width, g_height, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(g_canvas);
    }
    bsp_display_unlock();
    if (!g_canvas) {
        ESP_LOGE(TAG, "Could not create camera canvas");
        return false;
    }
    ESP_LOGI(TAG, "Onboard LVGL canvas initialized");
    return true;
}

bool onboard_view_show(const void *frame_bytes, size_t length,
                       uint8_t buffer_index) {
    if (!g_canvas || !frame_bytes || buffer_index >= 2 || !length) {
        ESP_LOGE(TAG, "Camera preview received invalid framebuffer");
        return false;
    }
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Cannot lock LVGL for camera frame");
        return false;
    }
    bool ok = onboard_ppa_convert(frame_bytes,
                      g_canvas_pixels[buffer_index], g_capacity);
    if (ok) {
        // Only the displayed RGB565 pixels are modified after PPA output.
        // The V4L2 camera buffer, PPA rotation, battery and charging state
        // are deliberately not changed by this diagnostic.
        ok = onboard_color_apply_rgb565(
            (uint16_t *)g_canvas_pixels[buffer_index],
            (size_t)g_width * g_height,
            onboard_color_ui_mode());
        if (!ok) {
            ESP_LOGE(TAG, "RGB565 color diagnostic transform failed");
        }
    }
    if (ok) {
#if CONFIG_LASR_USB_PREVIEW_DIAGNOSTIC
        // Subsample the SAME corrected, rotated LCD pixels before the
        // canvas advances. The serial worker owns its independent copy.
        lasr_usb_preview_offer(
            (const uint16_t *)g_canvas_pixels[buffer_index],
            g_width, g_height);
#endif
        lv_canvas_set_buffer(g_canvas, g_canvas_pixels[buffer_index],
                             g_width, g_height, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(g_canvas);
        lv_obj_invalidate(g_canvas);
    }
    bsp_display_unlock();
    return ok;
}
