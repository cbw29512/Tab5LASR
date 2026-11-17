#include "onboard_capture_buffers.h"
#include "esp_log.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

static const char *TAG = "lasr_csi_buf";

bool onboard_init_buffers(int fd, capture_buf_t *buffers) {
    struct v4l2_requestbuffers req = {
        .count = ONBOARD_BUF_COUNT, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP
    };
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < ONBOARD_BUF_COUNT) {
        ESP_LOGE(TAG, "V4L2 camera did not allocate two capture buffers");
        return false;
    }
    for (uint32_t i = 0; i < ONBOARD_BUF_COUNT; i++) {
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

void onboard_release_buffers(capture_buf_t *buffers) {
    for (int i = 0; i < ONBOARD_BUF_COUNT; i++) {
        if (buffers[i].data && buffers[i].data != MAP_FAILED) {
            if (munmap(buffers[i].data, buffers[i].size) != 0)
                ESP_LOGW(TAG, "munmap capture buffer failed");
            buffers[i].data = NULL;
        }
    }
}

