#include "lasr_web_frame.h"
#include "driver/jpeg_encode.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define LASR_MAX_JPEG (512u * 1024u)
static const char *TAG = "lasr_jpeg";
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_encoder_task;
static jpeg_encoder_handle_t s_encoder;
static uint16_t *s_pending_rgb, *s_encode_rgb;
static uint8_t *s_encode_out, *s_latest;
static size_t s_input_size, s_out_capacity;
static uint32_t s_w, s_h, s_sequence;
static size_t s_latest_size;
static int64_t s_last_encoded_us, s_last_offered_us;
static bool s_pending;

static void encode_worker(void *unused) {
    (void)unused;
    const jpeg_encode_cfg_t cfg = {
        .src_type = JPEG_ENCODE_IN_FORMAT_RGB565,
        .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
        .image_quality = 65,
        .width = s_w, .height = s_h
    };
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(300)) != pdTRUE) {
            ESP_LOGE(TAG, "Cannot acquire camera RGB frame");
            continue;
        }
        const bool ready = s_pending;
        if (ready) memcpy(s_encode_rgb, s_pending_rgb, s_input_size);
        s_pending = false;
        xSemaphoreGive(s_lock);
        if (!ready) continue;

        uint32_t encoded = 0;
        const esp_err_t err = jpeg_encoder_process(s_encoder, &cfg,
            (const uint8_t *)s_encode_rgb, s_input_size,
            s_encode_out, s_out_capacity, &encoded);
        if (err != ESP_OK || encoded == 0 || encoded > LASR_MAX_JPEG) {
            ESP_LOGE(TAG, "JPEG encode failed: %s; bytes=%lu",
                     esp_err_to_name(err), (unsigned long)encoded);
            continue;
        }
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(300)) != pdTRUE) {
            ESP_LOGE(TAG, "Cannot publish encoded frame");
            continue;
        }
        memcpy(s_latest, s_encode_out, encoded);
        s_latest_size = encoded;
        s_last_encoded_us = esp_timer_get_time();
        s_sequence++;
        xSemaphoreGive(s_lock);
    }
}

bool lasr_web_frame_init(uint32_t width, uint32_t height) {
    if (width < 320 || height < 240 || s_lock) {
        ESP_LOGE(TAG, "Invalid or repeated JPEG pipeline initialization");
        return false;
    }
    s_w = (width / 2u) & ~15u;
    s_h = (height / 2u) & ~15u;
    s_input_size = (size_t)s_w * s_h * 2u;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;
    s_pending_rgb = heap_caps_malloc(s_input_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_latest = heap_caps_malloc(LASR_MAX_JPEG, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const jpeg_encode_memory_alloc_cfg_t in = {.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER};
    const jpeg_encode_memory_alloc_cfg_t out = {.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER};
    size_t alloc_in = 0;
    s_encode_rgb = jpeg_alloc_encoder_mem(s_input_size, &in, &alloc_in);
    s_encode_out = jpeg_alloc_encoder_mem(LASR_MAX_JPEG, &out, &s_out_capacity);
    const jpeg_encode_engine_cfg_t engine = {.timeout_ms = 300};
    if (!s_pending_rgb || !s_latest || !s_encode_rgb || !s_encode_out ||
        alloc_in < s_input_size || s_out_capacity < LASR_MAX_JPEG ||
        jpeg_new_encoder_engine(&engine, &s_encoder) != ESP_OK) {
        ESP_LOGE(TAG, "Could not allocate hardware JPEG pipeline");
        return false;
    }
    if (xTaskCreate(encode_worker, "lasr_jpeg", 8192, NULL, 3,
                    &s_encoder_task) != pdPASS) {
        ESP_LOGE(TAG, "Could not start hardware JPEG encoder worker");
        return false;
    }
    ESP_LOGI(TAG, "Hardware MJPEG image %lux%lu @ <=4 fps; raw camera remains independent",
             (unsigned long)s_w, (unsigned long)s_h);
    return true;
}

void lasr_web_frame_offer(const uint16_t *pixels, uint32_t width, uint32_t height) {
    const int64_t now = esp_timer_get_time();
    if (!pixels || !s_lock || width < s_w * 2u || height < s_h * 2u ||
        now - s_last_offered_us < 250000) return;
    if (xSemaphoreTake(s_lock, 0) != pdTRUE) return; // Never stall CSI.
    if (s_pending) { xSemaphoreGive(s_lock); return; }
    for (uint32_t y = 0; y < s_h; ++y) {
        for (uint32_t x = 0; x < s_w; ++x) {
            s_pending_rgb[(size_t)y * s_w + x] =
                pixels[(size_t)(y * 2u) * width + (x * 2u)];
        }
    }
    s_pending = true;
    s_last_offered_us = now;
    xSemaphoreGive(s_lock);
    xTaskNotifyGive(s_encoder_task);
}

size_t lasr_web_frame_copy(uint8_t *dst, size_t cap,
                           uint32_t *seq, int64_t *age_us) {
    if (!dst || !s_lock || !cap ||
        xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) != pdTRUE) return 0;
    const size_t bytes = s_latest_size <= cap ? s_latest_size : 0;
    if (bytes) memcpy(dst, s_latest, bytes);
    if (seq) *seq = s_sequence;
    if (age_us) *age_us = s_last_encoded_us ?
        esp_timer_get_time() - s_last_encoded_us : -1;
    xSemaphoreGive(s_lock);
    return bytes;
}

uint32_t lasr_web_frame_count(void) { return s_sequence; }
