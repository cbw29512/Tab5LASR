#include "lasr_concurrent_sd.h"
#include "lasr_sd_smoke.h"
#include "lasr_sd_status_ui.h"
#include "onboard_state.h"
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

    // Status is PASS only after checking that new camera frames continue
    // to arrive AFTER the storage task finishes. This is a physical
    // concurrency test, not a one-time camera-startup assertion.
    const uint32_t before = __atomic_load_n(
        &g_onboard_status.captured_frames, __ATOMIC_ACQUIRE);
    vTaskDelay(pdMS_TO_TICKS(700));
    const uint32_t after = __atomic_load_n(
        &g_onboard_status.captured_frames, __ATOMIC_ACQUIRE);
    const bool camera_live = after > before;
    ESP_LOGI(TAG, "Post-SD camera frames: before=%lu after=%lu",
             (unsigned long)before, (unsigned long)after);
    if (!camera_live) {
        ESP_LOGE(TAG, "CAMERA+SD TEST FAILED: CSI stopped after SD operation");
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "CAMERA+SD TEST FAILED: SD (%s)",
                 esp_err_to_name(result));
    }
    const bool passed = result == ESP_OK && camera_live;
    if (passed) {
        ESP_LOGW(TAG, "CAMERA+SD TEST PASS: synthetic SD verified, CSI still streaming");
    }
    lasr_sd_status_ui_show(passed);
    vTaskDelete(NULL);
}

bool lasr_concurrent_sd_start(void) {
    if (s_started) {
        ESP_LOGE(TAG, "Refusing to repeat concurrent SD test in this boot");
        return false;
    }
    s_started = true; // One attempt, even if task allocation fails.
    // Equal priority with the normal main task prevents SD writes from
    // monopolizing the core while physical CSI frames are being captured.
    if (xTaskCreate(run_sd_probe, "lasr_camera_sd",
                    LASR_CONCURRENT_STACK_BYTES, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot allocate independent SD probe task; no SD test ran");
        return false;
    }
    ESP_LOGI(TAG, "Concurrent SD probe scheduled after CSI streaming began");
    return true;
}
