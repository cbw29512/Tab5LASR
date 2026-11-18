#include "onboard_camera.h"
#include "tab5_board_power.h"
#include "tab5_power.h"
#include "tab5_battery_monitor.h"
#include "tab5_power_widget.h"
#include "onboard_color_ui.h"
#include "lasr_sd_service.h"
#include "lasr_sd_widget.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "lasr_onboard";

void app_main(void) {
    // Preserve the proven power sequencing: never move SD ahead of I2C/power guards.
    lv_display_t *display = bsp_display_start();
    if (!display) { ESP_LOGE(TAG,"Tab5 display startup failed"); return; }
    if (!tab5_board_poweroff_idle_init()) {
        ESP_LOGE(TAG,"Power latch P4 idle LOW not verified; camera disabled");return;
    }
    if (!tab5_power_init()) {
        ESP_LOGW(TAG,"Battery charger not enabled: recovery may be necessary");
    }
    if (!tab5_battery_monitor_start()) {
        ESP_LOGE(TAG,"Battery diagnostic task unavailable");return;
    }
    if (bsp_display_brightness_set(80) != ESP_OK) {
        ESP_LOGW(TAG,"Display brightness configuration failed");
    }
#if CONFIG_LASR_SD_RECORDER
    // Filesystem I/O runs on its own task; camera acquisition never blocks on SD.
    if (!lasr_sd_service_init()) {
        ESP_LOGE(TAG,"SD writer unavailable: LASR must show UNSAVED");
    }
#endif
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG,"LVGL lock unavailable: battery recovery UI missing");return;
    }
    tab5_power_widget_attach(lv_scr_act());
    if (!onboard_color_ui_attach(lv_scr_act())) {
        ESP_LOGE(TAG,"Color diagnostic UI unavailable; original preview remains valid");
    }
#if CONFIG_LASR_SD_RECORDER
    if (!lasr_sd_widget_attach(lv_scr_act())) {
        ESP_LOGE(TAG,"SD status widget failed; no storage-ready claim");
    }
#endif
    bsp_display_unlock();
    ESP_LOGI(TAG,"Onboard MIPI CSI only. USB webcam is not used.");
    if (!onboard_camera_run()) {
        ESP_LOGE(TAG,"Built-in camera failed; see CSI sensor/format errors");
    }
}