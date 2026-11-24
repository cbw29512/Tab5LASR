#include "lasr_laser_pipeline.h"
#include <string.h>

bool lasr_laser_pipeline_init(lasr_laser_pipeline_t *p,
                              lasr_blob_config_t blob, lasr_pulse_config_t pulse) {
    if (!p) return false;
    memset(p,0,sizeof(*p));
    p->blob=blob;
    return lasr_pulse_init(&p->pulse,pulse);
}

bool lasr_laser_pipeline_step(lasr_laser_pipeline_t *p,
                              const lasr_raw_frame_t *now,
                              const lasr_raw_frame_t *dark,
                              uint64_t capture_us, lasr_pulse_event_t *out) {
    if (!p || !out) return false;
    *out=(lasr_pulse_event_t){0};
    lasr_blob_result_t found={0};
    if (!lasr_blob_scan(now,dark,&p->blob,&found))return false;
    ++p->scanned_frames;
    if (found.visible)++p->bright_frames;
    // Debounce is source-neutral, and time starts at first qualifying frame.
    return lasr_pulse_step(&p->pulse,capture_us,found.visible,
                            found.x_norm,found.y_norm,out);
}