#include "onboard_camera.h"
#include "onboard_state.h"
#include "onboard_stats.h"
#include "onboard_view.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define BUF_COUNT 2
static const char *TAG = "lasr_csi";
onboard_camera_status_t g_onboard_status = {.state = ONBOARD_BOOT};

typedef struct { void *data; size_t size; } capture_buf_t;

static bool init_buffers(int fd, capture_buf_t *buffers) {
    struct v4l2_requestbuffers req = {
        .count = BUF_COUNT, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP
    };
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < BUF_COUNT) {
        ESP_LOGE(TAG, "V4L2 camera did not allocate two capture buffers");
        return false;
    }
    for (uint32_t i = 0; i < BUF_COUNT; i++) {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP, .index = i
        };
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ESP_LOGE(TAG, "VIDIOC_QUERYBUF index=%lu failed", (unsigned long)i);
            return false;
        }
        buffers[i].size = buf.length;
        buffers[i].data = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].data == MAP_FAILED || !buffers[i].data) {
            ESP_LOGE(TAG, "mmap camera buffer index=%lu failed", (unsigned long)i);
            buffers[i].data = NULL;
            return false;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF index=%lu failed", (unsigned long)i);
            return false;
        }
    }
    return true;
}

static void release_buffers(capture_buf_t *buffers) {
    for (int i = 0; i < BUF_COUNT; i++) {
        if (buffers[i].data && buffers[i].data != MAP_FAILED) {
            if (munmap(buffers[i].data, buffers[i].size) != 0)
                ESP_LOGW(TAG, "munmap capture buffer failed");
            buffers[i].data = NULL;
        }
    }
}

static bool capture_frames(int fd, capture_buf_t *buffers) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
        return false;
    }
    const int64_t began = esp_timer_get_time();
    int64_t sampled = began;
    uint32_t prev_rx = 0, prev_draw = 0;
    g_onboard_status.state = ONBOARD_STREAMING;
    for (;;) {
        struct v4l2_buffer frame = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP
        };
        if (ioctl(fd, VIDIOC_DQBUF, &frame) < 0) {
            ESP_LOGE(TAG, "VIDIOC_DQBUF failed; stopped capture");
            break;
        }
        if (frame.index >= BUF_COUNT || frame.bytesused > buffers[frame.index].size) {
            ESP_LOGE(TAG, "Invalid capture buffer index or length");
            break;
        }
        g_onboard_status.captured_frames++;
        g_onboard_status.last_frame_monotonic_us = esp_timer_get_time();
        if (onboard_view_show(buffers[frame.index].data, frame.bytesused,
                              (uint8_t)frame.index)) {
            g_onboard_status.displayed_frames++;
        } else {
            g_onboard_status.failed_frames++;
        }
        if (ioctl(fd, VIDIOC_QBUF, &frame) < 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed after preview");
            break;
        }
        const int64_t now = esp_timer_get_time();
        if (now - sampled >= 5000000) {
            ESP_LOGI(TAG, "MIPI CSI %lux%lu fourcc=0x%08lx capture=%.1f FPS preview=%.1f FPS errors=%lu",
                     (unsigned long)g_onboard_status.width,
                     (unsigned long)g_onboard_status.height,
                     (unsigned long)g_onboard_status.pixel_format,
                     onboard_frame_rate(g_onboard_status.captured_frames - prev_rx, now - sampled),
                     onboard_frame_rate(g_onboard_status.displayed_frames - prev_draw, now - sampled),
                     (unsigned long)g_onboard_status.failed_frames);
            prev_rx = g_onboard_status.captured_frames;
            prev_draw = g_onboard_status.displayed_frames;
            sampled = now;
        }
    }
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) ESP_LOGE(TAG, "STREAMOFF failed");
    return false;
}

bool onboard_camera_run(void) {
    g_onboard_status.state = ONBOARD_CAMERA_INIT;
    esp_err_t err = bsp_camera_start(NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BSP MIPI CSI camera initialization failed: %s", esp_err_to_name(err));
        g_onboard_status.state = ONBOARD_ERROR;
        return false;
    }
    const int fd = open(BSP_CAMERA_DEVICE, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Could not open onboard V4L2 CSI device");
        g_onboard_status.state = ONBOARD_ERROR;
        return false;
    }
    struct v4l2_format fmt = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        ESP_LOGE(TAG, "Cannot read onboard sensor format");
        close(fd); g_onboard_status.state = ONBOARD_ERROR;
        return false;
    }
    g_onboard_status.width = fmt.fmt.pix.width;
    g_onboard_status.height = fmt.fmt.pix.height;
    g_onboard_status.pixel_format = fmt.fmt.pix.pixelformat;
    g_onboard_status.state = ONBOARD_FORMAT_READY;
    ESP_LOGI(TAG, "Sensor %lux%lu FOURCC=0x%08lx",
             (unsigned long)g_onboard_status.width,
             (unsigned long)g_onboard_status.height,
             (unsigned long)g_onboard_status.pixel_format);
    capture_buf_t buffers[BUF_COUNT] = {0};
    bool ok = onboard_view_init(g_onboard_status.width,
                               g_onboard_status.height,
                               g_onboard_status.pixel_format);
    if (ok) ok = init_buffers(fd, buffers);
    if (ok) ok = capture_frames(fd, buffers);
    release_buffers(buffers);
    if (close(fd) != 0) ESP_LOGW(TAG, "Camera close failed");
    g_onboard_status.state = ONBOARD_ERROR;
    return ok;
}
