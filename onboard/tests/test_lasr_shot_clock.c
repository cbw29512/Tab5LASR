#include "../main/lasr_shot_clock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const lasr_pulse_event_t first={true,4001000,.5f,.25f};
int main(void) {
    lasr_shot_clock_t c;lasr_clock_reset(&c);
    lasr_shot_t shot={0};
    assert(!lasr_clock_unscored_hit(&c,&first,&shot));
    assert(lasr_clock_countdown(&c,1000000,3000));
    assert(!lasr_clock_mark_go(&c,3999999));
    assert(lasr_clock_mark_go(&c,4000000));
    assert(lasr_clock_unscored_hit(&c,&first,&shot));
    assert(shot.sequence==1 && shot.elapsed_ms==1 && shot.split_ms==1);
    assert(strcmp(shot.zone,"UNSCORED")==0 && shot.score==0);
    assert(!lasr_clock_unscored_hit(&c,&first,&shot)); // duplicate frame
    assert(!lasr_clock_unscored_hit(&c,&(lasr_pulse_event_t){true,3999999,.4f,.5f},&shot));
    assert(lasr_clock_pause(&c,5000000));
    assert(!lasr_clock_unscored_hit(&c,&(lasr_pulse_event_t){true,5200000,.5f,.5f},&shot));
    assert(lasr_clock_countdown(&c,6000000,3000));
    assert(lasr_clock_mark_go(&c,9000000));
    assert(lasr_clock_unscored_hit(&c,&(lasr_pulse_event_t){true,9300000,.5f,.5f},&shot));
    assert(shot.sequence==2 && shot.elapsed_ms==1300 && shot.split_ms==1299);
    assert(lasr_clock_finish(&c,9500000));
    assert(!lasr_clock_unscored_hit(&c,&first,&shot));
    assert(!lasr_clock_countdown(&c,10000000,3000));
    puts("LASR monotonic GO/shot/split: pause, resume, duplicate suppression PASS");
}