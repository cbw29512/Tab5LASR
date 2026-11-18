#include "lasr_laser_pulse.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

bool lasr_pulse_init(lasr_pulse_detector_t *d, lasr_pulse_config_t c) {
    if (!d || !c.on_frames || !c.off_frames || c.on_frames > 20 ||
        c.off_frames > 20 || !c.max_gap_us) return false;
    memset(d, 0, sizeof(*d));
    d->config = c;
    d->phase = LASR_PULSE_WAIT_DARK; // A light already ON at startup is not a shot.
    return true;
}

static bool valid_position(float x, float y) {
    return isfinite(x) && isfinite(y) && x >= 0 && x <= 1 && y >= 0 && y <= 1;
}

bool lasr_pulse_step(lasr_pulse_detector_t *d, uint64_t t,
                     bool visible, float x, float y, lasr_pulse_event_t *out) {
    if (!d || !out || !t || (visible && !valid_position(x, y))) return false;
    *out = (lasr_pulse_event_t){0};
    // A dropped camera frame or clock discontinuity means we must see DARK again.
    if (d->last_frame_us && (t <= d->last_frame_us ||
        t - d->last_frame_us > d->config.max_gap_us)) {
        d->phase = LASR_PULSE_WAIT_DARK;
        d->dark_count = d->visible_count = 0;
        d->last_frame_us = t;
        return false;
    }
    d->last_frame_us = t;
    switch (d->phase) {
        case LASR_PULSE_WAIT_DARK:
            if (visible) { d->dark_count = 0; break; }
            if (++d->dark_count >= d->config.off_frames)
                d->phase = LASR_PULSE_ARMED;
            break;
        case LASR_PULSE_ARMED:
            if (visible) {
                d->first_visible_us = t; d->first_x = x; d->first_y = y;
                d->visible_count = 1;
                d->phase = LASR_PULSE_CANDIDATE;
            }
            break;
        case LASR_PULSE_CANDIDATE:
            if (!visible) {
                d->phase = LASR_PULSE_WAIT_DARK;
                d->dark_count = 1; d->visible_count = 0;
            } else if (d->visible_count < d->config.on_frames) {
                ++d->visible_count;
            }
            break;
        case LASR_PULSE_HELD:
            if (!visible) {
                d->phase = LASR_PULSE_REARMING;
                d->dark_count = 1;
                if (d->dark_count >= d->config.off_frames)
                    d->phase = LASR_PULSE_ARMED;
            }
            break;
        case LASR_PULSE_REARMING:
            if (visible) { d->phase = LASR_PULSE_HELD; d->dark_count = 0; }
            else if (++d->dark_count >= d->config.off_frames)
                d->phase = LASR_PULSE_ARMED;
            break;
        default:
            d->phase = LASR_PULSE_WAIT_DARK;
            return false;
    }
    if (d->phase == LASR_PULSE_CANDIDATE &&
        d->visible_count >= d->config.on_frames) {
        // Report first frame's timestamp to avoid debounce-dependent shot lag.
        *out = (lasr_pulse_event_t){true, d->first_visible_us, d->first_x, d->first_y};
        d->phase = LASR_PULSE_HELD;
    }
    return true;
}