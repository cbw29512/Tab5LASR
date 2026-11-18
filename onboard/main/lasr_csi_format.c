#include "lasr_csi_format.h"
#include <time.h>
#include <linux/videodev2.h>
#include <stddef.h>

bool lasr_csi_format_map(uint32_t fourcc, lasr_raw_format_t *out) {
    if (!out) return false;
    switch (fourcc) {
        case V4L2_PIX_FMT_RGB565: *out=LASR_RAW_RGB565_LE;return true;
        case V4L2_PIX_FMT_RGB565X:*out=LASR_RAW_RGB565_BE;return true;
        case V4L2_PIX_FMT_UYVY: *out=LASR_RAW_UYVY;return true;
        default: return false;
    }
}