#include "lasr_recorder_task.h"
#include "lasr_sd_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define LASR_QUEUE_LENGTH 32U
static const char *TAG = "lasr_writer";
static QueueHandle_t s_queue;
static volatile int s_status = LASR_REC_IDLE;
static volatile int s_accepting;
static volatile uint32_t s_dropped;
static volatile int s_finish_queued;
static volatile uint32_t s_inflight_shots;

static void writer_task(void *unused) {
    (void)unused;
    lasr_recorder_core_t core;
    lasr_recorder_core_init(&core);
    lasr_rec_message_t message;
    for (;;) {
        if (xQueueReceive(s_queue, &message, portMAX_DELAY) != pdTRUE) continue;
        // Check every wakeup. A full queue never silently produces a SAVED log.
        const uint32_t lost = __atomic_load_n(&s_dropped, __ATOMIC_RELAXED);
        if (lost && core.state == LASR_REC_RECORDING)
            lasr_recorder_core_lost_events(&core, lost);
        if (message.type == LASR_REC_BEGIN && !lasr_sd_is_mounted()) {
            const esp_err_t err = lasr_sd_mount_noformat();
            if (err != ESP_OK) {
                core.state = LASR_REC_UNSAVED;
                ESP_LOGE(TAG, "Session not saved; SD mount failed: %s", esp_err_to_name(err));
                __atomic_store_n(&s_accepting, 0, __ATOMIC_RELEASE);
                __atomic_store_n(&s_status, core.state, __ATOMIC_RELEASE);
                continue;
            }
        }
        const bool ok = lasr_recorder_core_apply(&core, &message, lasr_sd_root());
        if (!ok) ESP_LOGE(TAG, "Session command %d failed; UNSAVED or invalid state", message.type);
        if (message.type == LASR_REC_FINISH || core.state == LASR_REC_UNSAVED)
            __atomic_store_n(&s_accepting, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&s_status, core.state, __ATOMIC_RELEASE);
    }
}

bool lasr_recorder_task_init(void) {
    if (s_queue) return false;
    s_queue = xQueueCreate(LASR_QUEUE_LENGTH, sizeof(lasr_rec_message_t));
    if (!s_queue) { ESP_LOGE(TAG, "Unable to allocate bounded shot queue"); return false; }
    if (xTaskCreate(writer_task, "lasr_sd_owner", 6144, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Unable to create SD owner task");
        vQueueDelete(s_queue); s_queue = NULL; return false;
    }
    return true;
}

bool lasr_recorder_task_begin(const char *id, const char *target,
                              uint32_t revision, uint32_t delay) {
    if (!s_queue || !id || !target || strlen(id) > 40 || strlen(target) > 40) return false;
    const int state = __atomic_load_n(&s_status, __ATOMIC_ACQUIRE);
    if (state != LASR_REC_IDLE && state != LASR_REC_SAVED && state != LASR_REC_UNSAVED) return false;
    lasr_rec_message_t m = {.type = LASR_REC_BEGIN};
    snprintf(m.payload.begin.session_id, sizeof(m.payload.begin.session_id), "%s", id);
    snprintf(m.payload.begin.target_id, sizeof(m.payload.begin.target_id), "%s", target);
    m.payload.begin.calibration_revision = revision; m.payload.begin.delay_ms = delay;
    int expected = state;
    if (!__atomic_compare_exchange_n(&s_status, &expected, LASR_REC_STARTING,
                                      false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return false;
    __atomic_store_n(&s_dropped, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&s_finish_queued, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&s_accepting, 1, __ATOMIC_RELEASE);
    if (xQueueSend(s_queue, &m, 0) != pdTRUE) {
        __atomic_store_n(&s_accepting, 0, __ATOMIC_RELEASE);
        __atomic_store_n(&s_status, state, __ATOMIC_RELEASE); return false;
    }
    return true;
}

bool lasr_recorder_task_shot(const lasr_shot_t *shot) {
    if (!s_queue || !shot) return false;
    // In-flight reservation prevents END overtaking a concurrently accepted shot.
    __atomic_add_fetch(&s_inflight_shots, 1U, __ATOMIC_ACQ_REL);
    if (!__atomic_load_n(&s_accepting, __ATOMIC_ACQUIRE)) {
        __atomic_sub_fetch(&s_inflight_shots, 1U, __ATOMIC_ACQ_REL);
        return false;
    }
    lasr_rec_message_t m = {.type = LASR_REC_SHOT};
    m.payload.shot = *shot;
    const bool ok = xQueueSend(s_queue, &m, 0) == pdTRUE; // Never stall CSI capture.
    if (!ok) __atomic_add_fetch(&s_dropped, 1U, __ATOMIC_RELAXED);
    __atomic_sub_fetch(&s_inflight_shots, 1U, __ATOMIC_ACQ_REL);
    return ok;
}

bool lasr_recorder_task_finish(void) {
    if (!s_queue) return false;
    const int status = __atomic_load_n(&s_status, __ATOMIC_ACQUIRE);
    if (status != LASR_REC_STARTING && status != LASR_REC_RECORDING) return false;
    int expected = 0;
    if (!__atomic_compare_exchange_n(&s_finish_queued, &expected, 1, false,
                                      __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return false;
    // Close intake first, then wait for pre-existing senders to finish enqueueing.
    __atomic_store_n(&s_accepting, 0, __ATOMIC_RELEASE);
    for (unsigned ticks = 0; ticks < 100 &&
         __atomic_load_n(&s_inflight_shots, __ATOMIC_ACQUIRE); ++ticks)
        vTaskDelay(pdMS_TO_TICKS(1));
    if (__atomic_load_n(&s_inflight_shots, __ATOMIC_ACQUIRE)) {
        // Even if a delayed producer queues after END, SAVED is forbidden.
        __atomic_add_fetch(&s_dropped, 1U, __ATOMIC_RELAXED);
        ESP_LOGE(TAG, "Shot sender did not quiesce: session marked UNSAVED");
    }
    lasr_rec_message_t m = {.type = LASR_REC_FINISH};
    if (xQueueSend(s_queue, &m, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Finish queue saturated; retry stop; session remains UNSAVED");
        __atomic_store_n(&s_finish_queued, 0, __ATOMIC_RELEASE);
        return false;
    }
    return true;
}

lasr_rec_state_t lasr_recorder_task_status(void) {
    return (lasr_rec_state_t)__atomic_load_n(&s_status, __ATOMIC_ACQUIRE);
}
uint32_t lasr_recorder_task_dropped(void) {
    return __atomic_load_n(&s_dropped, __ATOMIC_ACQUIRE);
}