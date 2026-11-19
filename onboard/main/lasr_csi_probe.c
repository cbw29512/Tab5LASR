#include "lasr_csi_probe.h"
#include "sdkconfig.h"
#include "lasr_csi_format.h"
#include "lasr_laser_pipeline.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG="lasr_csi_probe";
static uint8_t *reference;
static size_t reference_length, stride_bytes;
static uint32_t source_width, source_height;
static lasr_raw_format_t source_format;
static lasr_laser_pipeline_t pipeline;
static bool reference_set;
static uint32_t candidate_events;
static uint32_t last_scan_us, worst_scan_us;
static int s_recalibrate_on_next_frame;
// The combined hardware test starts with the scanner disabled so that live
// preview FPS can be compared with/without expensive raw pixel analysis.
// Standalone raw-CSI diagnostics retain the existing scanner-on behavior.
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
#define LASR_SCAN_DEFAULT 0
#define LASR_SCAN_STEP 3
#else
#define LASR_SCAN_DEFAULT 1
#define LASR_SCAN_STEP 1
#endif
static int s_scanning_enabled = LASR_SCAN_DEFAULT;

void lasr_csi_probe_set_scanning_enabled(bool enabled) {
    // A fresh ON transition requires an untouched, laser-OFF reference frame.
    // Let the capture task perform this reset; LVGL never mutates the camera
    // baseline or the pulse detector concurrently.
    if (enabled) __atomic_store_n(&s_recalibrate_on_next_frame, 1,
                                  __ATOMIC_RELEASE);
    __atomic_store_n(&s_scanning_enabled, enabled ? 1 : 0, __ATOMIC_RELEASE);
    ESP_LOGW(TAG, "Raw CSI candidate scanner %s (NO real shots stored)",
             enabled ? "ON" : "OFF");
}

bool lasr_csi_probe_scanning_enabled(void) {
    return __atomic_load_n(&s_scanning_enabled, __ATOMIC_ACQUIRE) != 0;
}

void lasr_csi_probe_end(void) {
    if (reference) heap_caps_free(reference);
    reference=NULL;reference_set=false;candidate_events=0;
    last_scan_us=worst_scan_us=0;
    __atomic_store_n(&s_recalibrate_on_next_frame, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&s_scanning_enabled,
                     LASR_SCAN_DEFAULT, __ATOMIC_RELEASE);
    reference_length=0;stride_bytes=0;source_width=source_height=0;
}

bool lasr_csi_probe_begin(uint32_t w, uint32_t h,
                          uint32_t fourcc, uint32_t row_stride) {
    lasr_csi_probe_end();
    if (!lasr_csi_format_map(fourcc,&source_format) || !w || !h ||
        w>4096 || h>4096 || w>SIZE_MAX/2u) {
        ESP_LOGE(TAG,"Unsupported CSI format or dimensions for read-only probe");
        return false;
    }
    const size_t stride=row_stride?row_stride:(size_t)w*2;
    if (stride < (size_t)w*2 || stride%2 || h>SIZE_MAX/stride) {
        ESP_LOGE(TAG,"Invalid CSI row stride for read-only probe");
        return false;
    }
    // These are provisional diagnostics only, NOT a calibrated SIRT threshold.
    const lasr_blob_config_t blob={.left=.1f,.top=.1f,.right=.9f,
        .bottom=.9f,.min_luma=180,.min_delta=90,.step=LASR_SCAN_STEP,
        .min_cluster=3,.radius_px=4,.max_candidates=120};
    if (!lasr_laser_pipeline_init(&pipeline,blob,
        (lasr_pulse_config_t){.on_frames=2,.off_frames=2,.max_gap_us=150000})) {
        ESP_LOGE(TAG,"Cannot initialize synthetic laser pulse thresholds");
        return false;
    }
    reference_length=stride*(size_t)h;
    reference=heap_caps_malloc(reference_length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!reference) {
        ESP_LOGE(TAG,"Cannot allocate %zu-byte CSI baseline; probe disabled",reference_length);
        lasr_csi_probe_end();return false;
    }
    source_width=w;source_height=h;stride_bytes=stride;
    ESP_LOGW(TAG,"Uncalibrated CSI ROI step=%u; dense spot check follows coarse candidates",
             (unsigned)LASR_SCAN_STEP);
    ESP_LOGW(TAG,"OPT-IN DIAGNOSTIC only: first frame becomes UNVERIFIED dark reference");
    ESP_LOGW(TAG,"NO scoring, NO microSD logging, NO SIRT calibration certified");
    return true;
}

bool lasr_csi_probe_observe(const void *borrowed,size_t length,uint64_t timestamp_us) {
    if (!reference || !borrowed || length<reference_length || !timestamp_us)
        return false;
    // Always return the borrowed V4L2 buffer promptly when scanning is OFF.
    // The UI can enable the full uncalibrated scanner for an A/B FPS test.
    if (!lasr_csi_probe_scanning_enabled()) return true;
    if (__atomic_exchange_n(&s_recalibrate_on_next_frame, 0,
                            __ATOMIC_ACQ_REL)) {
        reference_set = false;
        if (!lasr_pulse_init(&pipeline.pulse, pipeline.pulse.config)) {
            ESP_LOGE(TAG, "Cannot re-arm CSI pulse scanner for fresh baseline");
            return false;
        }
        ESP_LOGW(TAG, "Scanner enabled; collecting fresh UNVERIFIED dark baseline");
    }
    if (!reference_set) {
        // Copy before the capture loop requeues its V4L2-owned memory.
        memcpy(reference,borrowed,reference_length);
        reference_set=true;
        ESP_LOGI(TAG,"Copied first CSI frame to independent PSRAM reference");
        return true;
    }
    const lasr_raw_frame_t now={borrowed,length,stride_bytes,
                                source_width,source_height,source_format};
    const lasr_raw_frame_t dark={reference,reference_length,stride_bytes,
                                 source_width,source_height,source_format};
    lasr_pulse_event_t event={0};
    const int64_t started = esp_timer_get_time();
    const bool scanned = lasr_laser_pipeline_step(
        &pipeline,&now,&dark,timestamp_us,&event);
    const int64_t elapsed = esp_timer_get_time() - started;
    last_scan_us = elapsed > 0 && elapsed <= UINT32_MAX ?
        (uint32_t)elapsed : UINT32_MAX;
    if (last_scan_us > worst_scan_us) worst_scan_us = last_scan_us;
    if (!scanned) return false;
    if (event.shot_detected) {
        ++candidate_events;
        ESP_LOGW(TAG,"UNVERIFIED candidate t=%" PRIu64 "us x=%.4f y=%.4f (not recorded)",
                 event.first_seen_us,(double)event.x_norm,(double)event.y_norm);
    }
    // Periodic observability distinguishes a camera with no candidate spots
    // from one seeing bright frames that do not pass debounce. No fake shots.
    if (pipeline.scanned_frames &&
        (pipeline.scanned_frames % 10U) == 0U) {
        // Measure real scanner cost to distinguish sensor/DQBUF delays from
        // full-frame CPU scanning; report in milliseconds, not guessed FPS.
        ESP_LOGI(TAG, "CSI probe frames=%lu bright=%lu candidates=%lu scan_last_us=%lu worst_us=%lu",
                 (unsigned long)pipeline.scanned_frames,
                 (unsigned long)pipeline.bright_frames,
                 (unsigned long)candidate_events,
                 (unsigned long)last_scan_us,
                 (unsigned long)worst_scan_us);
    }
    return true;
}