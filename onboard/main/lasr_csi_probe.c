#include "lasr_csi_probe.h"
#include "lasr_csi_format.h"
#include "lasr_laser_pipeline.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
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

void lasr_csi_probe_end(void) {
    if (reference) heap_caps_free(reference);
    reference=NULL;reference_set=false;candidate_events=0;
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
        .bottom=.9f,.min_luma=180,.min_delta=90,.step=1,
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
    ESP_LOGW(TAG,"OPT-IN DIAGNOSTIC only: first frame becomes UNVERIFIED dark reference");
    ESP_LOGW(TAG,"NO scoring, NO microSD logging, NO SIRT calibration certified");
    return true;
}

bool lasr_csi_probe_observe(const void *borrowed,size_t length,uint64_t timestamp_us) {
    if (!reference || !borrowed || length<reference_length || !timestamp_us)
        return false;
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
    if (!lasr_laser_pipeline_step(&pipeline,&now,&dark,timestamp_us,&event))
        return false;
    if (event.shot_detected) {
        ++candidate_events;
        ESP_LOGW(TAG,"UNVERIFIED candidate t=%" PRIu64 "us x=%.4f y=%.4f (not recorded)",
                 event.first_seen_us,(double)event.x_norm,(double)event.y_norm);
    }
    // Periodic observability distinguishes a camera with no candidate spots
    // from one seeing bright frames that do not pass debounce. No fake shots.
    if (pipeline.scanned_frames &&
        (pipeline.scanned_frames % 120U) == 0U) {
        ESP_LOGI(TAG, "CSI probe frames=%lu bright=%lu pulse_candidates=%lu",
                 (unsigned long)pipeline.scanned_frames,
                 (unsigned long)pipeline.bright_frames,
                 (unsigned long)candidate_events);
    }
    return true;
}