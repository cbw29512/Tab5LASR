#include "onboard_camera.h"
#include "tab5_board_power.h"
#include "tab5_power.h"
#include "tab5_battery_monitor.h"
#include "tab5_power_widget.h"
#include "onboard_color_ui.h"
#include "sdkconfig.h"
#if CONFIG_LASR_SD_SMOKE_ON_BOOT
#include "lasr_sd_smoke.h"
#endif
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "lasr_onboard";

void app_main(void) {
    // Camera and charger share the Tab5 board's I2C/expander setup.
    // Display startup initializes the I2C bus before any P4/P5/P7 changes.
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
    // Always create the guarded recovery control even if camera init fails.
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
    // Explicit test build only. Never format the card or overwrite sessions.
    if (lasr_sd_smoke_once() != ESP_OK)
        ESP_LOGE(TAG, "Opt-in microSD test failed; camera will still start");
#endif
    ESP_LOGI(TAG, "Onboard MIPI CSI only. USB webcam is not used.");
    if (!onboard_camera_run()) {
        ESP_LOGE(TAG, "Built-in camera failed; see CSI sensor/format errors");
    }
}
