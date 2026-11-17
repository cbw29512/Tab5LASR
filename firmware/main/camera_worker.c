#include "camera_capture.h"
#include "camera_metrics.h"
#include "camera_preview.h"
#include "camera_state.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

static const char *TAG = "lasr_fps";

void camera_preview_worker(void *arg) {
    const esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)arg;
    int64_t last_us = esp_timer_get_time();
    uint32_t last_captured = 0, last_drawn = 0;

    for (;;) {
        camera_frame_t owned = {0};
        if (xQueueReceive(g_camera_frame_queue, &owned,
                          pdMS_TO_TICKS(250)) == pdPASS) {
            // JPEG data belongs to us even if the USB camera disconnects.
            bool ok = camera_preview_draw(panel, owned.bytes, owned.length);
            if (ok) {
                __atomic_fetch_add(&g_camera_stats.decoded, 1, __ATOMIC_RELAXED);
                __atomic_fetch_add(&g_camera_stats.displayed, 1, __ATOMIC_RELAXED);
            } else {
                __atomic_fetch_add(&g_camera_stats.decode_errors, 1, __ATOMIC_RELAXED);
            }
            heap_caps_free(owned.bytes);
        }
        const int64_t now_us = esp_timer_get_time();
        if (now_us - last_us >= 5000000) {
            const uint32_t captured = __atomic_load_n(
                &g_camera_stats.captured, __ATOMIC_RELAXED);
            const uint32_t rendered = __atomic_load_n(
                &g_camera_stats.displayed, __ATOMIC_RELAXED);
            ESP_LOGI(TAG,
                "state=%d captured=%.1f FPS preview=%.1f FPS dropped=%lu decode_fail=%lu",
                (int)g_camera_stats.phase,
                camera_rate_fps(captured - last_captured, now_us - last_us),
                camera_rate_fps(rendered - last_drawn, now_us - last_us),
                (unsigned long)__atomic_load_n(&g_camera_stats.dropped, __ATOMIC_RELAXED),
                (unsigned long)__atomic_load_n(&g_camera_stats.decode_errors, __ATOMIC_RELAXED));
            last_us = now_us;
            last_captured = captured;
            last_drawn = rendered;
        }
    }
}
