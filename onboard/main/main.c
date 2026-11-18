#include "onboard_camera.h"
#if CONFIG_LASR_WEB_ENABLED
#include "lasr_web_wifi.h"
#endif
#include "tab5_board_power.h"
#include "tab5_power.h"
#include "tab5_battery_monitor.h"
#include "tab5_power_widget.h"
#include "onboard_color_ui.h"
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

#if CONFIG_LASR_WEB_ENABLED
    // Network initialization runs separately; CSI and battery remain usable
    // even if the factory C6 firmware does not match this ESP-Hosted host.
    if (!lasr_web_wifi_start()) {
        ESP_LOGE(TAG, "LAN viewer disabled; local camera still available");
    }
#endif
    ESP_LOGI(TAG, "Onboard MIPI CSI only. USB webcam is not used.");
    if (!onboard_camera_run()) {
        ESP_LOGE(TAG, "Built-in camera failed; see CSI sensor/format errors");
    }
}
