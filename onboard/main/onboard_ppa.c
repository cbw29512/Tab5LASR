#include "onboard_ppa.h"
#include "bsp/esp-bsp.h"
#include "driver/ppa.h"
#include "esp_log.h"
#include <linux/videodev2.h>

static const char *TAG = "lasr_ppa";
static ppa_client_handle_t g_ppa;
static ppa_srm_color_mode_t g_input_mode;
static ppa_srm_rotation_angle_t g_rotation;
static bool g_swap;
static uint32_t g_width, g_height, g_out_w, g_out_h;

bool onboard_ppa_init(uint32_t width, uint32_t height,
                      uint32_t pixel_format, uint32_t *out_w, uint32_t *out_h) {
    if (!out_w || !out_h || width == 0 || height == 0 ||
        width > 2048 || height > 2048) {
        ESP_LOGE(TAG, "Invalid onboard capture resolution");
        return false;
    }
    g_width = width; g_height = height;
    g_swap = false;
    g_rotation = PPA_SRM_ROTATION_ANGLE_270;
    if (pixel_format == V4L2_PIX_FMT_RGB565X) {
        g_input_mode = PPA_SRM_COLOR_MODE_RGB565;
        g_swap = true;
    } else if (pixel_format == V4L2_PIX_FMT_RGB565) {
        g_input_mode = PPA_SRM_COLOR_MODE_RGB565;
    } else if (pixel_format == V4L2_PIX_FMT_UYVY) {
        g_input_mode = PPA_SRM_COLOR_MODE_YUV422_UYVY;
        g_rotation = PPA_SRM_ROTATION_ANGLE_0; // Espressif's default UYVY path
    } else {
        ESP_LOGE(TAG, "Unsupported onboard pixel format 0x%08lx",
                 (unsigned long)pixel_format);
        return false;
    }

    const uint32_t rotate = (g_rotation == PPA_SRM_ROTATION_ANGLE_90 ||
                             g_rotation == PPA_SRM_ROTATION_ANGLE_270);
    const uint32_t src_w = rotate ? height : width;
    const uint32_t src_h = rotate ? width : height;
    const float scale_w = (float)BSP_LCD_H_RES / src_w;
    const float scale_h = (float)BSP_LCD_V_RES / src_h;
    const float scale = scale_w < scale_h ? scale_w : scale_h;
    g_out_w = (uint32_t)(src_w * scale);
    g_out_h = (uint32_t)(src_h * scale);
    if (!g_out_w || !g_out_h) {
        ESP_LOGE(TAG, "Preview scaling produced zero dimension");
        return false;
    }
    const ppa_client_config_t cfg = {.oper_type = PPA_OPERATION_SRM};
    esp_err_t err = ppa_register_client(&cfg, &g_ppa);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not register PPA scaler: %s", esp_err_to_name(err));
        return false;
    }
    *out_w = g_out_w; *out_h = g_out_h;
    ESP_LOGI(TAG, "PPA preview %lux%lu from %lux%lu",
             (unsigned long)g_out_w, (unsigned long)g_out_h,
             (unsigned long)width, (unsigned long)height);
    return true;
}

bool onboard_ppa_convert(const void *input, void *output, size_t output_bytes) {
    if (!g_ppa || !input || !output ||
        output_bytes < (size_t)g_out_w * g_out_h * 2) {
        ESP_LOGE(TAG, "PPA buffers unavailable or too small");
        return false;
    }
    ppa_srm_oper_config_t cfg = {
        .in = {
            .buffer = (void *)input,
            .pic_w = g_width, .pic_h = g_height,
            .block_w = g_width, .block_h = g_height,
            .block_offset_x = 0, .block_offset_y = 0,
            .srm_cm = g_input_mode, .yuv_range = PPA_COLOR_RANGE_FULL,
            .yuv_std = PPA_COLOR_CONV_STD_RGB_YUV_BT601,
        },
        .out = {
            .buffer = output, .buffer_size = output_bytes,
            .pic_w = g_out_w, .pic_h = g_out_h,
            .block_offset_x = 0, .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = g_rotation,
        .scale_x = (float)g_out_w / ((g_rotation == PPA_SRM_ROTATION_ANGLE_0 ||
                                    g_rotation == PPA_SRM_ROTATION_ANGLE_180) ?
                                    g_width : g_height),
        .scale_y = (float)g_out_h / ((g_rotation == PPA_SRM_ROTATION_ANGLE_0 ||
                                    g_rotation == PPA_SRM_ROTATION_ANGLE_180) ?
                                    g_height : g_width),
        .rgb_swap = 0, .byte_swap = g_swap,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    const esp_err_t err = ppa_do_scale_rotate_mirror(g_ppa, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PPA conversion error: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
