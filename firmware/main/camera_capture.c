/* Camera capture based on Espressif USB Host UVC example (Apache-2.0). */
#include "camera_capture.h"
#include "camera_state.h"
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/uvc_host.h"

static const char *TAG = "lasr_capture";
QueueHandle_t g_camera_frame_queue;
camera_stats_t g_camera_stats = { .phase = CAMERA_DISCONNECTED };
static volatile bool g_connected;

static bool on_frame(const uvc_host_frame_t *frame, void *ctx) {
    (void)ctx;
    __atomic_fetch_add(&g_camera_stats.captured, 1, __ATOMIC_RELAXED);
    if (!frame || !frame->data || !frame->data_len ||
        frame->data_len > LASR_MAX_JPEG_BYTES) {
        __atomic_fetch_add(&g_camera_stats.dropped, 1, __ATOMIC_RELAXED);
        return true; // Return UVC frame buffer immediately.
    }
    camera_frame_t owned = {
        .bytes = heap_caps_malloc(frame->data_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        .length = frame->data_len,
        .captured_us = esp_timer_get_time()
    };
    if (!owned.bytes) {
        __atomic_fetch_add(&g_camera_stats.dropped, 1, __ATOMIC_RELAXED);
        return true;
    }
    memcpy(owned.bytes, frame->data, frame->data_len);
    if (xQueueSend(g_camera_frame_queue, &owned, 0) != pdPASS) {
        heap_caps_free(owned.bytes);
        __atomic_fetch_add(&g_camera_stats.dropped, 1, __ATOMIC_RELAXED);
    }
    return true; // The receiver owns its copy, never the UVC callback buffer.
}

static void on_event(const uvc_host_stream_event_data_t *event, void *ctx) {
    (void)ctx;
    switch (event->type) {
    case UVC_HOST_DEVICE_DISCONNECTED:
        ESP_LOGW(TAG, "USB camera disconnected");
        g_connected = false;
        g_camera_stats.phase = CAMERA_DISCONNECTED;
        if (uvc_host_stream_close(event->device_disconnected.stream_hdl) != ESP_OK)
            ESP_LOGE(TAG, "Camera stream close failed on unplug");
        break;
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGE(TAG, "USB transfer error: %d", event->transfer_error.error);
        break;
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "Driver frame overflow: camera MJPEG exceeds negotiated frame buffer");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGW(TAG, "Driver frame underflow: USB capture outruns processing");
        break;
    default:
        ESP_LOGW(TAG, "USB camera event code %d", event->type);
        break;
    }
}

void camera_usb_library_task(void *ctx) {
    (void)ctx;
    for (;;) {
        uint32_t flags = 0;
        const esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "USB Host event handling failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            const esp_err_t cleanup = usb_host_device_free_all();
            if (cleanup != ESP_OK) {
                ESP_LOGW(TAG, "USB device cleanup deferred: %s", esp_err_to_name(cleanup));
            }
        }
    }
}

void camera_capture_task(void *ctx) {
    (void)ctx;
    const uvc_host_stream_config_t config = {
        .event_cb = on_event, .frame_cb = on_frame,
        .usb = { .vid = UVC_HOST_ANY_VID, .pid = UVC_HOST_ANY_PID,
                 .uvc_stream_index = 0 },
        .vs_format = { .h_res = LASR_CAM_W, .v_res = LASR_CAM_H,
                       .fps = LASR_CAM_FPS, .format = UVC_VS_FORMAT_MJPEG },
        .advanced = {
            .frame_size = 0, .number_of_frame_buffers = 3,
            .number_of_urbs = 8, .urb_size = 8192,
            .frame_heap_caps = MALLOC_CAP_SPIRAM
        }
    };
    for (;;) {
        uvc_host_stream_hdl_t stream = NULL;
        g_camera_stats.phase = CAMERA_NEGOTIATING;
        ESP_LOGI(TAG, "Searching for any MJPEG UVC webcam: 320x240 @15 fps");
        const esp_err_t open = uvc_host_stream_open(&config, pdMS_TO_TICKS(5000), &stream);
        if (open != ESP_OK) {
            g_camera_stats.phase = CAMERA_WAITING;
            ESP_LOGW(TAG, "Camera negotiation failed: %s; retry in 3s",
                     esp_err_to_name(open));
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }
        g_connected = true;
        const esp_err_t start = uvc_host_stream_start(stream);
        if (start != ESP_OK) {
            ESP_LOGE(TAG, "UVC stream start failed: %s", esp_err_to_name(start));
            g_connected = false;
            g_camera_stats.phase = CAMERA_ERROR;
            if (uvc_host_stream_close(stream) != ESP_OK)
                ESP_LOGE(TAG, "UVC stream cleanup failed");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }
        g_camera_stats.phase = CAMERA_STREAMING;
        ESP_LOGI(TAG, "Camera is streaming; measuring real capture/render FPS");
        while (g_connected) vTaskDelay(pdMS_TO_TICKS(250));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
