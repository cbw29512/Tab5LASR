#include "camera_preview.h"
#include "camera_state.h"
#include "tab5_board_power.h"
#include "tab5_power.h"
#include "tab5_battery_monitor.h"
#include "esp_log.h"

static const char *TAG = "tab5lasr";

void app_main(void) {
    esp_lcd_panel_handle_t panel = NULL;
    if (!camera_preview_open(&panel)) {
        ESP_LOGE(TAG, "ABORT: camera preview display initialization failed");
        return;
    }
    // Exact Tab5 dice-roller power initialization order; never pulse P4 HIGH.
    if (!tab5_board_poweroff_idle_init()) {
        ESP_LOGE(TAG, "Power-off latch failed to initialize; aborting USB");
        return;
    }
    if (!tab5_power_init()) {
        // Never bypass charger safety to make a camera demo work.
        ESP_LOGW(TAG, "Battery charging not enabled; check battery status");
    }
    if (!tab5_battery_monitor_start()) {
        ESP_LOGW(TAG, "Battery telemetry unavailable; do not certify battery portability");
    }
    if (!camera_uvc_start(panel)) {
        ESP_LOGE(TAG, "Camera USB host initialization failed");
        return;
    }
    ESP_LOGI(TAG, "Camera-only capability test initialized (no dice code)");
}
