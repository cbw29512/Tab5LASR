#include "lasr_shot_clock.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

void lasr_clock_reset(lasr_shot_clock_t *c) {
    if (c) {memset(c,0,sizeof(*c));c->phase=LASR_CLOCK_IDLE;}
}

bool lasr_clock_countdown(lasr_shot_clock_t *c,
                          uint64_t start_us, uint32_t delay_ms) {
    if (!c || !start_us || (delay_ms!=3000 && delay_ms!=5000 &&
        delay_ms!=10000) || (c->phase!=LASR_CLOCK_IDLE &&
        c->phase!=LASR_CLOCK_PAUSED) ||
        start_us>UINT64_MAX-(uint64_t)delay_ms*1000u) return false;
    c->phase=LASR_CLOCK_COUNTDOWN;
    c->countdown_end_us=start_us+(uint64_t)delay_ms*1000u;
    c->delay_ms=delay_ms;
    return true;
}

bool lasr_clock_mark_go(lasr_shot_clock_t *c,uint64_t actual_beep_us) {
    if (!c || c->phase!=LASR_CLOCK_COUNTDOWN ||
        actual_beep_us<c->countdown_end_us) return false;
    // Use ACTUAL beep presentation instant rather than assumed countdown end.
    c->go_us=actual_beep_us;
    c->phase=LASR_CLOCK_ACTIVE;
    return true;
}

bool lasr_clock_pause(lasr_shot_clock_t *c,uint64_t now_us) {
    if (!c || c->phase!=LASR_CLOCK_ACTIVE || now_us<c->go_us ||
        UINT64_MAX-c->accumulated_us<now_us-c->go_us) return false;
    c->accumulated_us+=now_us-c->go_us;
    c->phase=LASR_CLOCK_PAUSED;
    return true;
}

bool lasr_clock_finish(lasr_shot_clock_t *c,uint64_t now_us) {
    if (!c || c->phase==LASR_CLOCK_IDLE ||
        c->phase==LASR_CLOCK_COMPLETE) return false;
    if (c->phase==LASR_CLOCK_ACTIVE && !lasr_clock_pause(c,now_us))
        return false;
    c->phase=LASR_CLOCK_COMPLETE;
    return true;
}

bool lasr_clock_unscored_hit(lasr_shot_clock_t *c,
                            const lasr_pulse_event_t *pulse,lasr_shot_t *out) {
    if (!c || !pulse || !out || !pulse->shot_detected ||
        c->phase!=LASR_CLOCK_ACTIVE || pulse->first_seen_us<c->go_us ||
        pulse->first_seen_us<=c->last_event_us ||
        !isfinite(pulse->x_norm) || !isfinite(pulse->y_norm) ||
        pulse->x_norm<0 || pulse->x_norm>1 ||
        pulse->y_norm<0 || pulse->y_norm>1 || c->count==UINT32_MAX)
        return false;
    const uint64_t span=pulse->first_seen_us-c->go_us;
    if (c->accumulated_us>UINT64_MAX-span) return false;
    const uint64_t total_ms=(c->accumulated_us+span)/1000u;
    if (total_ms>UINT32_MAX || (c->count && total_ms<c->last_shot_elapsed_ms))
        return false;
    const uint32_t elapsed=(uint32_t)total_ms;
    *out=(lasr_shot_t){.sequence=c->count+1,.elapsed_ms=elapsed,
        .split_ms=elapsed-c->last_shot_elapsed_ms,.score=0,
        .x_norm=pulse->x_norm,.y_norm=pulse->y_norm};
    // Explicitly unscored until a confirmed target homography is available.
    memcpy(out->zone,"UNSCORED",sizeof("UNSCORED"));
    c->count++;c->last_shot_elapsed_ms=elapsed;
    c->last_event_us=pulse->first_seen_us;
    return true;
}