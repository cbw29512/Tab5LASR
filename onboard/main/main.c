#include "onboard_camera.h"
#include "tab5_board_power.h"
#include "tab5_power.h"
#include "tab5_battery_monitor.h"
#include "tab5_power_widget.h"
#include "onboard_color_ui.h"
#include "sdkconfig.h"
#if CONFIG_LASR_SD_SMOKE_ON_BOOT
#include "lasr_sd_smoke.h"
#include "lasr_sd_status_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "lasr_onboard";

#if CONFIG_LASR_SD_SMOKE_ON_BOOT
// ESP-IDF FreeRTOS expects the task stack size in BYTES.
// Run filesystem operations outside the small app_main task stack.
// This is opt-in SD-only diagnostic code; normal camera boot is unchanged.
#define LASR_SD_TASK_STACK_BYTES (24U * 1024U)

static void lasr_sd_diagnostic_task(void *unused) {
    (void)unused;
    ESP_LOGI(TAG, "SD-ONLY diagnostic task starting (stack=%u bytes)",
             (unsigned)LASR_SD_TASK_STACK_BYTES);
    const esp_err_t result = lasr_sd_smoke_once();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "microSD diagnostic FAILED: %s", esp_err_to_name(result));
    } else {
        ESP_LOGW(TAG, "microSD file and synthetic session diagnostic PASSED");
    }
    lasr_sd_status_ui_show(result == ESP_OK);
    ESP_LOGI(TAG, "SD diagnostic task completed; task will exit");
    vTaskDelete(NULL);
}
#endif

void app_main(void) {
    // Display initializes I2C before the guarded power-latch operations.
    lv_display_t *display = bsp_display_start();
    if (!display) {
        ESP_LOGE(TAG, "Tab5 display startup failed; camera not started");
        return;
    }
    if (!tab5_board_poweroff_idle_init()) {
        ESP_LOGE(TAG, "Power latch P4 idle LOW not verified; camera disabled");
        return;
    }
    if (!tab5_power_init()) {
        ESP_LOGW(TAG, "Battery charger not enabled: recovery may be necessary");
    }
    if (!tab5_battery_monitor_start()) {
        ESP_LOGE(TAG, "Battery diagnostic task unavailable");
        return;
    }
    if (bsp_display_brightness_set(80) != ESP_OK) {
        ESP_LOGW(TAG, "Display brightness configuration failed");
    }
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "LVGL lock unavailable: battery recovery UI missing");
        return;
    }
    tab5_power_widget_attach(lv_scr_act());
    if (!onboard_color_ui_attach(lv_scr_act())) {
        ESP_LOGE(TAG, "Color diagnostic UI unavailable; original camera preview remains valid");
    }
    bsp_display_unlock();

#if CONFIG_LASR_SD_SMOKE_ON_BOOT
    // Test-only build: camera capture intentionally disabled. A synthetic
    // diagnostic session is NOT physical laser scoring or recording.
    ESP_LOGW(TAG, "SD-ONLY DIAGNOSTIC: CSI camera deliberately disabled");
    // A dedicated task prevents nested FAT/session calls from exhausting
    // the main ESP-IDF task stack (physically observed on Tab5, Nov 18).
    // Return from app_main after spawning; LVGL and battery tasks stay alive.
    if (xTaskCreate(lasr_sd_diagnostic_task, "lasr_sd_diag",
                    LASR_SD_TASK_STACK_BYTES, NULL,
                    tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not allocate SD diagnostic task stack");
        lasr_sd_status_ui_show(false);
    }
#else
    ESP_LOGI(TAG, "Onboard MIPI CSI only. USB webcam is not used.");
    if (!onboard_camera_run()) {
        ESP_LOGE(TAG, "Built-in camera failed; see CSI sensor/format errors");
    }
#endif
}