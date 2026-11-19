#include "lasr_concurrent_sd.h"
#include "lasr_sd_smoke.h"
#include "lasr_sd_status_ui.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Same stack isolation that passed the first on-device SD diagnostic.
// ESP-IDF FreeRTOS task stack argument is measured in BYTES.
#define LASR_CONCURRENT_STACK_BYTES (24U * 1024U)
static const char *TAG = "lasr_camera_sd";
static bool s_started;

static void run_sd_probe(void *arg) {
    (void)arg;
    ESP_LOGW(TAG, "CAMERA+SD TEST starting: camera is already streaming");
    ESP_LOGW(TAG, "Synthetic storage only; SIRT candidates are NOT saved");
    const esp_err_t result = lasr_sd_smoke_once();
    if (result == ESP_OK) {
        ESP_LOGW(TAG, "CAMERA+SD TEST: SD persistence PASS; verify CSI streaming separately");
    } else {
        ESP_LOGE(TAG, "CAMERA+SD TEST: SD FAIL (%s)", esp_err_to_name(result));
    }
    lasr_sd_status_ui_show(result == ESP_OK);
    vTaskDelete(NULL);
}

bool lasr_concurrent_sd_start(void) {
    if (s_started) {
        ESP_LOGE(TAG, "Refusing to repeat concurrent SD test in this boot");
        return false;
    }
    s_started = true; // One attempt, even if task allocation fails.
    if (xTaskCreate(run_sd_probe, "lasr_camera_sd",
                    LASR_CONCURRENT_STACK_BYTES, NULL,
                    tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot allocate independent SD probe task; no SD test ran");
        return false;
    }
    ESP_LOGI(TAG, "Concurrent SD probe scheduled after CSI streaming began");
    return true;
}
