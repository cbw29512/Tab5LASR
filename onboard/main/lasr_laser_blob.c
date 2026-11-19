#include "lasr_laser_blob.h"
#include <math.h>
#include <string.h>

static bool valid_config(const lasr_blob_config_t *c) {
    return c && isfinite(c->left) && isfinite(c->right) &&
        isfinite(c->top) && isfinite(c->bottom) &&
        c->left>=0 && c->top>=0 && c->right<=1 && c->bottom<=1 &&
        c->right>c->left && c->bottom>c->top && c->step>=1 &&
        c->step<=8 && c->min_cluster>0 && c->radius_px>=c->step &&
        c->radius_px<=64 &&
        c->min_luma>0 && c->min_delta>0 && c->max_candidates>0;
}
static bool candidate(const lasr_raw_frame_t *now,
                      const lasr_raw_frame_t *dark,
                      const lasr_blob_config_t *c,
                      uint32_t x, uint32_t y, uint8_t *delta) {
    // lasr_blob_scan validated BOTH frames, including stride and length,
    // and bounded this ROI before entering the scan loops. Avoid
    // revalidating complete V4L2 frame metadata for every single pixel.
    const uint8_t a = lasr_raw_luma_validated(now, x, y);
    // Most pixels cannot qualify: avoid reading / decoding a second
    // PSRAM frame unless the current pixel exceeds the bright threshold.
    if (a < c->min_luma) return false;
    const uint8_t background = lasr_raw_luma_validated(dark, x, y);
    *delta = a > background ? (uint8_t)(a - background) : 0;
    return *delta >= c->min_delta;
}
bool lasr_blob_scan(const lasr_raw_frame_t *now,
                    const lasr_raw_frame_t *dark,
                    const lasr_blob_config_t *cfg, lasr_blob_result_t *out) {
    if (out) *out=(lasr_blob_result_t){0};
    if (!out || !valid_config(cfg) || !lasr_raw_frame_valid(now) ||
        !lasr_raw_frame_valid(dark) || now->width!=dark->width ||
        now->height!=dark->height || now->format!=dark->format) return false;
    const uint32_t x0=(uint32_t)(cfg->left*now->width);
    const uint32_t y0=(uint32_t)(cfg->top*now->height);
    const uint32_t x1=(uint32_t)ceilf(cfg->right*now->width);
    const uint32_t y1=(uint32_t)ceilf(cfg->bottom*now->height);
    if (x0>=now->width || y0>=now->height || x1<=x0 || y1<=y0 ||
        x1>now->width || y1>now->height) return false;
    uint32_t peak_x=0,peak_y=0; uint8_t peak=0;
    for (uint32_t y=y0;y<y1;y+=cfg->step) {
        for (uint32_t x=x0;x<x1;x+=cfg->step) {
            uint8_t delta=0;
            if (!candidate(now,dark,cfg,x,y,&delta)) continue;
            if (++out->candidates>cfg->max_candidates) return true;
            if (delta>peak) {peak=delta;peak_x=x;peak_y=y;}
        }
    }
    if (!out->candidates) return true;
    // A candidate must be a compact spot, not scattered bright changes.
    uint64_t sx=0,sy=0; uint32_t clustered=0;
    const uint64_t radius2=(uint64_t)cfg->radius_px*cfg->radius_px;
    // Coarse ROI search may skip pixels for speed, but every detected
    // candidate receives a DENSE local check. A small 3x3 SIRT spot must
    // not lose its cluster merely because the coarse grid used step=2.
    // Restrict the dense work to radius <=64 (never a second full-ROI scan).
    const int32_t extent=(int32_t)cfg->radius_px;
    for (int32_t iy=-extent;iy<=extent;++iy) {
        const int32_t y=(int32_t)peak_y+iy;
        if (y<(int32_t)y0 || y>=(int32_t)y1) continue;
        for (int32_t ix=-extent;ix<=extent;++ix) {
            const int32_t x=(int32_t)peak_x+ix;
            if (x<(int32_t)x0 || x>=(int32_t)x1) continue;
            const int64_t dx=(int64_t)x-peak_x,dy=(int64_t)y-peak_y;
            if ((uint64_t)(dx*dx+dy*dy)>radius2) continue;
            uint8_t delta=0;
            if (candidate(now,dark,cfg,(uint32_t)x,(uint32_t)y,&delta)) {
                sx+=(uint32_t)x;sy+=(uint32_t)y;++clustered;
            }
        }
    }
    out->cluster=clustered;out->peak_delta=peak;
    if (clustered<cfg->min_cluster || clustered*2<=out->candidates)
        return true; // reject scattered/ambiguous changes
    out->visible=true;
    out->x_norm=((float)sx/clustered+0.5f)/now->width;
    out->y_norm=((float)sy/clustered+0.5f)/now->height;
    return true;
}