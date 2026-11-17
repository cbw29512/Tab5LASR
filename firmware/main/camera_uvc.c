#include "camera_state.h"
#include "camera_capture.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/uvc_host.h"

static const char *TAG = "lasr_usb";
extern void camera_preview_worker(void *arg);

bool camera_uvc_start(esp_lcd_panel_handle_t panel) {
    if (!panel) {
        ESP_LOGE(TAG, "Cannot start camera without LCD panel");
        return false;
    }
    esp_err_t err = bsp_feature_enable(BSP_FEATURE_USB, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB-A VBUS power setup failed: %s", esp_err_to_name(err));
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    g_camera_frame_queue = xQueueCreate(3, sizeof(camera_frame_t));
    if (!g_camera_frame_queue) {
        ESP_LOGE(TAG, "Could not create bounded JPEG queue");
        return false;
    }
    const usb_host_config_t host_config = {
        .skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB host install failed: %s", esp_err_to_name(err));
        return false;
    }
    if (xTaskCreate(camera_usb_library_task, "usb_events", 4096, NULL, 15, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create USB event daemon");
        return false;
    }
    const uvc_host_driver_config_t uvc = {
        .driver_task_stack_size = 4096, .driver_task_priority = 16,
        .xCoreID = tskNO_AFFINITY, .create_background_task = true,
    };
    err = uvc_host_install(&uvc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UVC driver install failed: %s", esp_err_to_name(err));
        return false;
    }
    if (xTaskCreate(camera_preview_worker, "camera_draw", 8192, panel, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create preview worker");
        return false;
    }
    if (xTaskCreate(camera_capture_task, "camera_uvc", 8192, NULL, 12, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot create UVC stream worker");
        return false;
    }
    return true;
}
