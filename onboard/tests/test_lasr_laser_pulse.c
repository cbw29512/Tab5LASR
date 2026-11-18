#include "../main/lasr_laser_pulse.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool frame(lasr_pulse_detector_t *d, uint64_t t, bool visible,
                  uint64_t *event_time) {
    lasr_pulse_event_t hit = {0};
    assert(lasr_pulse_step(d, t, visible, .51f, .49f, &hit));
    if (hit.shot_detected) {
        *event_time = hit.first_seen_us;
        assert(hit.x_norm > .50f && hit.y_norm < .50f);
    }
    return hit.shot_detected;
}
int main(void) {
    lasr_pulse_detector_t d;
    assert(!lasr_pulse_init(&d,(lasr_pulse_config_t){0,2,100000}));
    assert(lasr_pulse_init(&d,(lasr_pulse_config_t){2,2,100000}));
    uint64_t t = 0;
    // If laser is already visible at startup, it cannot fire until observed OFF.
    assert(!frame(&d,10000,true,&t));
    assert(!frame(&d,20000,true,&t));
    assert(!frame(&d,30000,false,&t));
    assert(!frame(&d,40000,false,&t));
    assert(!frame(&d,50000,true,&t));
    assert(frame(&d,60000,true,&t) && t==50000);
    for (uint64_t tick=70000;tick<=110000;tick+=10000)
        assert(!frame(&d,tick,true,&t));
    assert(!frame(&d,120000,false,&t));
    assert(!frame(&d,130000,true,&t)); // Flicker doesn't rearm.
    assert(!frame(&d,140000,false,&t));
    assert(!frame(&d,150000,false,&t));
    assert(!frame(&d,160000,true,&t));
    assert(frame(&d,170000,true,&t) && t==160000);
    lasr_pulse_event_t hit = {0};
    assert(!lasr_pulse_step(&d, 170000,true,.5f,.5f,&hit)); // Same timestamp invalid.
    assert(!hit.shot_detected);
    assert(!frame(&d,180000,false,&t));
    assert(!frame(&d,190000,false,&t));
    // Oversized camera gap means no firing until OFF is observed again.
    assert(!lasr_pulse_step(&d,400000,true,.5f,.5f,&hit));
    assert(!hit.shot_detected);
    assert(!frame(&d,410000,false,&t));
    assert(!frame(&d,420000,false,&t));
    assert(!frame(&d,430000,true,&t));
    assert(frame(&d,440000,true,&t) && t==430000);
    assert(!lasr_pulse_step(&d,450000,true,NAN,.5f,&hit));
    puts("LASR laser pulse: dark-first, one-shot, rearm, gaps, validation PASS");
    return 0;
}