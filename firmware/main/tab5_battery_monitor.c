#include "tab5_battery_monitor.h"
#include "tab5_power_diag.h"
#include "tab5_power.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lasr_battery";

// Battery telemetry state is independent of camera FPS and frame buffers.
static void monitor_task(void *unused) {
    (void)unused;
    for (;;) {
        const tab5_power_reading_t r = tab5_power_diag_read();
        char message[160] = {0};
        tab5_power_diag_format(message, sizeof(message), &r,
                               tab5_power_charge_command_enabled());
        if (r.error != ESP_OK || !r.voltage_valid) {
            ESP_LOGE(TAG, "Battery monitor unable to read INA226: %s",
                     esp_err_to_name(r.error));
        } else if (r.battery_mv > 8450 &&
                   tab5_power_charge_command_enabled()) {
            // A high bus-voltage reading must never trigger additional charging.
            ESP_LOGE(TAG, "Safety limit: battery bus %lu mV exceeds 8450 mV; disabling charge",
                     (unsigned long)r.battery_mv);
            if (!tab5_power_command_set(false)) {
                ESP_LOGE(TAG, "Could not verify charger-off command");
            }
        }
        ESP_LOGI(TAG, "%s", message);
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

bool tab5_battery_monitor_start(void) {
    if (xTaskCreate(monitor_task, "battery_diag", 4096, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start battery telemetry task");
        return false;
    }
    return true;
}
