#include "lasr_web_http.h"
#include "lasr_web_assets.h"
#include "lasr_web_frame.h"
#include "lasr_web_wifi.h"
#include "onboard_state.h"
#include "tab5_power_diag.h"
#include "tab5_power.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "lasr_http";
static httpd_handle_t s_control, s_stream_server;
static volatile unsigned s_stream_clients;
static volatile uint32_t s_sent_count;
static uint32_t s_last_capture, s_last_sent;
static int64_t s_last_sample_us;
static double s_fps_capture, s_fps_stream;

static esp_err_t status_handler(httpd_req_t *req) {
    char json[550] = {0};
    const int64_t now = esp_timer_get_time();
    if (now - s_last_sample_us > 2000000) {
        const int64_t window = now - s_last_sample_us;
        s_fps_capture = window > 0 ? (double)(g_onboard_status.captured_frames -
            s_last_capture) * 1000000.0 / window : 0;
        s_fps_stream = window > 0 ? (double)(s_sent_count -
            s_last_sent) * 1000000.0 / window : 0;
        s_last_capture = g_onboard_status.captured_frames;
        s_last_sent = s_sent_count;
        s_last_sample_us = now;
    }
    int64_t age = -1;
    const bool has_frame = (g_onboard_status.last_frame_monotonic_us > 0);
    if (has_frame) age = (now - g_onboard_status.last_frame_monotonic_us) / 1000;
    const char *state = g_onboard_status.state == ONBOARD_STREAMING ?
        (has_frame && age >= 0 && age < 3000 ? "live" : "stale") :
        (g_onboard_status.state == ONBOARD_ERROR ? "error" : "starting");
    const tab5_power_reading_t battery = tab5_power_diag_read();
    char battery_json[24] = "null";
    if (battery.error == ESP_OK && battery.voltage_valid)
        snprintf(battery_json, sizeof(battery_json), "%lu",
                 (unsigned long)battery.battery_mv);
    char age_json[24] = "null";
    if (age >= 0) snprintf(age_json, sizeof(age_json), "%lld", (long long)age);
    const int length = snprintf(json, sizeof(json),
        "{\"camera\":\"onboard_csi\",\"state\":\"%s\",\"width\":%lu,"
        "\"height\":%lu,\"fps_capture\":%.1f,\"fps_stream\":%.1f,"
        "\"frame_age_ms\":%s,\"dropped_frames\":%lu,"
        "\"battery_mv\":%s,\"charger_command_on\":%s,"
        "\"wifi_state\":\"%s\",\"error\":null}",
        state, (unsigned long)g_onboard_status.width,
        (unsigned long)g_onboard_status.height, s_fps_capture, s_fps_stream,
        age_json, (unsigned long)g_onboard_status.failed_frames,
        battery_json, tab5_power_charge_command_enabled() ? "true" : "false",
        lasr_web_wifi_ready() ? "connected" : "disconnected");
    if (length <= 0 || length >= (int)sizeof(json)) {
        ESP_LOGE(TAG, "Camera status JSON truncated");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Status too long");
    }
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, length);
}

static esp_err_t stream_handler(httpd_req_t *req) {
    if (__atomic_exchange_n(&s_stream_clients, 1, __ATOMIC_ACQ_REL)) {
        // ESP-IDF's httpd_err_code_t does not have a built-in 503 enum.
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "One camera viewer at a time");
    }
    uint8_t *jpeg = heap_caps_malloc(512u * 1024u, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!jpeg) {
        __atomic_store_n(&s_stream_clients, 0, __ATOMIC_RELEASE);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Insufficient JPEG buffer");
    }
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    uint32_t last_sequence = 0;
    esp_err_t result = ESP_OK;
    while (lasr_web_wifi_ready()) {
        uint32_t sequence = 0;
        int64_t age_us = -1;
        const size_t bytes = lasr_web_frame_copy(jpeg, 512u * 1024u,
                                                &sequence, &age_us);
        if (!bytes || sequence == last_sequence || age_us < 0 || age_us > 3000000) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        char header[100] = {0};
        const int size = snprintf(header, sizeof(header),
           "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n", bytes);
        if (size <= 0 || size >= (int)sizeof(header)) {
            ESP_LOGE(TAG, "MJPEG header truncated");
            result = ESP_FAIL;
            break;
        }
        if (httpd_resp_send_chunk(req, header, size) != ESP_OK ||
            httpd_resp_send_chunk(req, (char *)jpeg, bytes) != ESP_OK) {
            ESP_LOGI(TAG, "Browser camera stream disconnected");
            result = ESP_FAIL;
            break;
        }
        last_sequence = sequence;
        __atomic_fetch_add(&s_sent_count, 1, __ATOMIC_RELAXED);
    }
    heap_caps_free(jpeg);
    __atomic_store_n(&s_stream_clients, 0, __ATOMIC_RELEASE);
    return result;
}

bool lasr_web_http_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;
    if (httpd_start(&s_control, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Could not start LAN viewer on port 80");
        return false;
    }
    if (!lasr_web_assets_register(s_control)) return false;
    const httpd_uri_t status = {.uri="/api/status", .method=HTTP_GET,
                                .handler=status_handler};
    if (httpd_register_uri_handler(s_control, &status) != ESP_OK) return false;
    cfg.server_port = 81; cfg.ctrl_port += 1; cfg.stack_size = 8192;
    if (httpd_start(&s_stream_server, &cfg) != ESP_OK) return false;
    const httpd_uri_t stream = {.uri="/stream", .method=HTTP_GET,
                                .handler=stream_handler};
    if (httpd_register_uri_handler(s_stream_server, &stream) != ESP_OK) return false;
    ESP_LOGI(TAG, "LAN status: :80/api/status; camera MJPEG :81/stream");
    return true;
}
