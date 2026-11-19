#include "lasr_raw_pixel.h"
#include <stdint.h>

bool lasr_raw_frame_valid(const lasr_raw_frame_t *f) {
    if (!f || !f->data || !f->width || !f->height ||
        f->width > 4096 || f->height > 4096 ||
        (f->format != LASR_RAW_RGB565_LE &&
         f->format != LASR_RAW_RGB565_BE &&
         f->format != LASR_RAW_UYVY) || f->stride % 2 ||
        f->stride < (size_t)f->width * 2) return false;
    // Guard multiplication and the final row access, including padding.
    return f->height <= f->length / f->stride;
}

uint8_t lasr_raw_luma_validated(const lasr_raw_frame_t *f,
                                uint32_t x, uint32_t y) {
    // This decoder is hot-path only. Callers validate all buffer dimensions,
    // frame format, and bounds BEFORE the first pixel in a scan.
    const size_t offset = (size_t)y * f->stride + (size_t)x * 2;
    const uint8_t lo=f->data[offset], hi=f->data[offset+1];
    if (f->format == LASR_RAW_UYVY) {
        return hi; // U Y0 V Y1 packed 4:2:2, luminance at odd byte.
    }
    const uint16_t packed = f->format == LASR_RAW_RGB565_LE ?
        (uint16_t)(lo | ((uint16_t)hi << 8)) :
        (uint16_t)(((uint16_t)lo << 8) | hi);
    const uint32_t red=((packed >> 11) & 31u) * 255u / 31u;
    const uint32_t green=((packed >> 5) & 63u) * 255u / 63u;
    const uint32_t blue=(packed & 31u) * 255u / 31u;
    return (uint8_t)((77u*red+150u*green+29u*blue+128u)>>8);
}

bool lasr_raw_luma(const lasr_raw_frame_t *f,
                   uint32_t x, uint32_t y, uint8_t *out) {
    if (!out || !lasr_raw_frame_valid(f) ||
        x >= f->width || y >= f->height) return false;
    *out = lasr_raw_luma_validated(f, x, y);
    return true;
}