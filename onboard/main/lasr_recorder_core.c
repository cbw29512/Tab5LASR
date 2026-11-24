#include "lasr_recorder_core.h"
#include <stddef.h>
#include <string.h>

void lasr_recorder_core_init(lasr_recorder_core_t *core) {
    if (!core) return;
    memset(core, 0, sizeof(*core));
    core->state = LASR_REC_IDLE;
}

static bool fail_session(lasr_recorder_core_t *core) {
    if (!core) return false;
    // The pending journal remains on disk for recovery; never claim SAVED.
    lasr_store_abort(&core->store);
    core->state = LASR_REC_UNSAVED;
    return false;
}

void lasr_recorder_core_lost_events(lasr_recorder_core_t *core, uint32_t lost) {
    if (!core || !lost) return;
    if (UINT32_MAX - core->discarded_events < lost)
        core->discarded_events = UINT32_MAX;
    else core->discarded_events += lost;
    if (core->state == LASR_REC_RECORDING || core->state == LASR_REC_FINALIZING)
        (void)fail_session(core);
}

bool lasr_recorder_core_apply(lasr_recorder_core_t *core,
                             const lasr_rec_message_t *message, const char *root) {
    if (!core || !message || !root) return false;
    switch (message->type) {
        case LASR_REC_BEGIN:
            if (core->state != LASR_REC_IDLE && core->state != LASR_REC_SAVED &&
                core->state != LASR_REC_UNSAVED) return false;
            core->state = LASR_REC_STARTING;
            core->discarded_events = 0;
            if (!lasr_store_begin(&core->store, root, message->payload.begin.session_id,
                                  message->payload.begin.target_id,
                                  message->payload.begin.calibration_revision,
                                  message->payload.begin.delay_ms))
                return fail_session(core);
            core->state = LASR_REC_RECORDING;
            return true;
        case LASR_REC_SHOT:
            if (core->state != LASR_REC_RECORDING) return false;
            if (!lasr_store_append(&core->store, &message->payload.shot))
                return fail_session(core);
            return true;
        case LASR_REC_FINISH:
            if (core->state != LASR_REC_RECORDING || core->discarded_events)
                return false;
            core->state = LASR_REC_FINALIZING;
            if (!lasr_store_finish(&core->store)) return fail_session(core);
            core->state = LASR_REC_SAVED;
            return true;
        default:
            return fail_session(core);
    }
}