#include "onboard_camera.h"
#include "onboard_state.h"
#include "onboard_stats.h"
#include "onboard_view.h"
#include "onboard_capture_buffers.h"
#include "sdkconfig.h"
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
#include "lasr_concurrent_sd.h"
#endif
#if CONFIG_LASR_CSI_DIAGNOSTIC
#include "lasr_csi_probe.h"
#endif
#include "bsp/esp-bsp.h"
#include "esp_video_device.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

static const char *TAG = "lasr_csi";
onboard_camera_status_t g_onboard_status = {.state = ONBOARD_BOOT};
#if CONFIG_LASR_CSI_DIAGNOSTIC
static bool s_csi_probe_started;
#endif

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
#if CONFIG_LASR_CSI_DIAGNOSTIC
    bool warned_probe=false;
#endif
    for (;;) {
        struct v4l2_buffer frame = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP
        };
        if (ioctl(fd, VIDIOC_DQBUF, &frame) < 0) {
            ESP_LOGE(TAG, "VIDIOC_DQBUF failed; stopped capture");
            break;
        }
        if (frame.index >= ONBOARD_BUF_COUNT || frame.bytesused > buffers[frame.index].size) {
            ESP_LOGE(TAG, "Invalid capture buffer index or length");
            break;
        }
        g_onboard_status.captured_frames++;
        g_onboard_status.last_frame_monotonic_us = esp_timer_get_time();
#if CONFIG_LASR_CSI_DIAGNOSTIC
        // Observe raw V4L2-owned bytes BEFORE QBUF, not Color 2/4 LCD pixels.
        if (s_csi_probe_started &&
            !lasr_csi_probe_observe(buffers[frame.index].data, frame.bytesused,
                                    (uint64_t)g_onboard_status.last_frame_monotonic_us) &&
            !warned_probe) {
            ESP_LOGW(TAG, "Uncalibrated CSI diagnostics failed; video remains active");
            warned_probe=true;
        }
#endif
        if (onboard_view_show(buffers[frame.index].data, buffers[frame.index].size,
                              (uint8_t)frame.index)) {
            g_onboard_status.displayed_frames++;
        } else {
            g_onboard_status.failed_frames++;
        }
        if (ioctl(fd, VIDIOC_QBUF, &frame) < 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed after preview");
            break;
        }
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
        // Launch only after four real frames; SD runs in a separate task.
        if (g_onboard_status.captured_frames == 4U &&
            !lasr_concurrent_sd_start())
            ESP_LOGE(TAG, "Concurrent SD test task failed to start");
#endif
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
    capture_buf_t buffers[ONBOARD_BUF_COUNT] = {0};
    bool ok = onboard_view_init(g_onboard_status.width,
                               g_onboard_status.height,
                               g_onboard_status.pixel_format);
    if (ok) ok = onboard_init_buffers(fd, buffers);
#if CONFIG_LASR_CSI_DIAGNOSTIC
    if (ok) {
        s_csi_probe_started=lasr_csi_probe_begin(fmt.fmt.pix.width,
                 fmt.fmt.pix.height,fmt.fmt.pix.pixelformat,
                 fmt.fmt.pix.bytesperline);
        if (!s_csi_probe_started)
            ESP_LOGW(TAG, "Read-only CSI diagnostics disabled; camera remains usable");
    }
#endif
    if (ok) ok = capture_frames(fd, buffers);
#if CONFIG_LASR_CSI_DIAGNOSTIC
    lasr_csi_probe_end();s_csi_probe_started=false;
#endif
    onboard_release_buffers(buffers);
    if (close(fd) != 0) ESP_LOGW(TAG, "Camera close failed");
    g_onboard_status.state = ONBOARD_ERROR;
    return ok;
}
