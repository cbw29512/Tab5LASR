#define _POSIX_C_SOURCE 200809L
#include "../main/lasr_recorder_core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static lasr_rec_message_t start_command(const char *session) {
    lasr_rec_message_t m = {.type = LASR_REC_BEGIN};
    snprintf(m.payload.begin.session_id, sizeof(m.payload.begin.session_id), "%s", session);
    snprintf(m.payload.begin.target_id, sizeof(m.payload.begin.target_id), "bullseye");
    m.payload.begin.calibration_revision = 2;
    m.payload.begin.delay_ms = 3000;
    return m;
}
static lasr_rec_message_t shot_command(uint32_t sequence, uint32_t elapsed,
                                        uint32_t split) {
    lasr_rec_message_t m = {.type = LASR_REC_SHOT};
    m.payload.shot = (lasr_shot_t){sequence, elapsed, split, 10, .5f, .5f, "R1"};
    return m;
}
int main(void) {
    char root[] = "/tmp/lasr-recorder-core-XXXXXX";
    assert(mkdtemp(root));
    lasr_recorder_core_t core; lasr_recorder_core_init(&core);
    assert(core.state == LASR_REC_IDLE);
    lasr_rec_message_t m = shot_command(1, 700, 700);
    assert(!lasr_recorder_core_apply(&core, &m, root));
    m = start_command("session001");
    assert(lasr_recorder_core_apply(&core, &m, root));
    assert(core.state == LASR_REC_RECORDING);
    m = shot_command(1, 700, 700);
    assert(lasr_recorder_core_apply(&core, &m, root));
    m = shot_command(2, 1000, 300);
    assert(lasr_recorder_core_apply(&core, &m, root));
    m.type = LASR_REC_FINISH;
    assert(lasr_recorder_core_apply(&core, &m, root));
    assert(core.state == LASR_REC_SAVED);
    char file[300]; snprintf(file, sizeof(file), "%s/session.json", core.store.folder);
    assert(access(file, R_OK) == 0);
    m = start_command("session002");
    assert(lasr_recorder_core_apply(&core, &m, root));
    m = shot_command(1, 700, 700);
    assert(lasr_recorder_core_apply(&core, &m, root));
    lasr_recorder_core_lost_events(&core, 1);
    assert(core.state == LASR_REC_UNSAVED);
    assert(core.discarded_events == 1);
    m.type = LASR_REC_FINISH;
    assert(!lasr_recorder_core_apply(&core, &m, root));
    snprintf(file, sizeof(file), "%s/shots.pending", core.store.folder);
    assert(access(file, R_OK) == 0);
    snprintf(file, sizeof(file), "%s/session.json", core.store.folder);
    assert(access(file, F_OK) != 0);
    m = start_command("session003");
    assert(lasr_recorder_core_apply(&core, &m, root));
    m = shot_command(2, 700, 700); // Sequence 1 was never seen.
    assert(!lasr_recorder_core_apply(&core, &m, root));
    assert(core.state == LASR_REC_UNSAVED);
    m = start_command("session003"); // Previous partial directory retained.
    assert(!lasr_recorder_core_apply(&core, &m, root));
    puts("LASR recorder core: state, ordering, no overwrite, lost-event gate PASS");
    return 0;
}