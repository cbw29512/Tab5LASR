#include "camera_preview.h"
#include "camera_state.h"
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "jpeg_decoder.h"

static const char *TAG = "lasr_preview";
static uint16_t *g_rgb;

bool camera_preview_open(esp_lcd_panel_handle_t *panel_out) {
    if (!panel_out) {
        ESP_LOGE(TAG, "Missing display output pointer");
        return false;
    }
    esp_lcd_panel_io_handle_t io = NULL;
    bsp_display_config_t cfg = {
        .dsi_bus = {
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        },
    };
    esp_err_t err = bsp_display_new(&cfg, panel_out, &io);
    if (err != ESP_OK || !*panel_out) {
        ESP_LOGE(TAG, "Tab5 display setup failed: %s", esp_err_to_name(err));
        return false;
    }
    g_rgb = heap_caps_malloc(LASR_CAM_W * LASR_CAM_H * sizeof(uint16_t),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_rgb) {
        ESP_LOGE(TAG, "Could not allocate 320x240 RGB565 PSRAM buffer");
        return false;
    }
    err = bsp_display_brightness_set(80);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Brightness setup failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "Display initialized; preview region %dx%d",
             LASR_CAM_W, LASR_CAM_H);
    return true;
}

bool camera_preview_draw(esp_lcd_panel_handle_t panel,
                         const uint8_t *jpeg, size_t jpeg_bytes) {
    if (!panel || !jpeg || jpeg_bytes == 0 || jpeg_bytes > LASR_MAX_JPEG_BYTES || !g_rgb) {
        ESP_LOGE(TAG, "Invalid JPEG/frame buffer state");
        return false;
    }
    esp_jpeg_image_cfg_t cfg = {
        .indata = (uint8_t *)jpeg,
        .indata_size = jpeg_bytes,
        .outbuf = (uint8_t *)g_rgb,
        .outbuf_size = LASR_CAM_W * LASR_CAM_H * sizeof(uint16_t),
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .flags = { .swap_color_bytes = 1 },
    };
    esp_jpeg_image_output_t output = {0};
    esp_err_t err = esp_jpeg_decode(&cfg, &output);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MJPEG decode failed: %s", esp_err_to_name(err));
        return false;
    }
    const int x = (BSP_LCD_H_RES - LASR_CAM_W) / 2;
    const int y = (BSP_LCD_V_RES - LASR_CAM_H) / 2;
    err = esp_lcd_panel_draw_bitmap(panel, x, y,
                                    x + LASR_CAM_W, y + LASR_CAM_H, g_rgb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD draw failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
