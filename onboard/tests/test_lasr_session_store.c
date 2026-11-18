#define _POSIX_C_SOURCE 200809L
#include "lasr_session_store.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static lasr_shot_t shot(uint32_t n, uint64_t t, float x, float y, int value) {
    lasr_shot_t result = {.sequence=n, .elapsed_ms=t, .x_norm=x, .y_norm=y,
                          .score=value};
    (void)snprintf(result.zone, sizeof(result.zone), "CENTER");
    return result;
}

int main(void) {
    char root[]="/tmp/lasr-store-tests-XXXXXX";
    assert(mkdtemp(root));
    lasr_session_t session;
    assert(!lasr_store_begin(&session, root, "../../etc", 0, 3000));
    assert(lasr_store_begin(&session, root, "bullseye", 7, 3000));
    assert(session.phase == LASR_STORE_RECORDING);
    const lasr_shot_t first = shot(1, 720, .5f, .5f, 10);
    assert(lasr_store_shot(&session, &first));
    const lasr_shot_t duplicate = shot(1, 999, .5f, .5f, 10);
    assert(!lasr_store_shot(&session, &duplicate));
    lasr_shot_t past = shot(2, 500, .5f, .5f, 10);
    assert(!lasr_store_shot(&session, &past));
    lasr_shot_t invalid = shot(2, 900, NAN, .5f, 10);
    assert(!lasr_store_shot(&session, &invalid));
    lasr_shot_t next = shot(2, 1310, .44f, .49f, 9);
    assert(lasr_store_shot(&session, &next));
    assert(session.score_total == 19 && session.shot_count == 2);
    assert(!lasr_store_finish(&session, 1300));
    assert(lasr_store_finish(&session, 2000));
    assert(session.phase == LASR_STORE_SAVED);
    assert(lasr_store_verify_saved(session.directory, 2));
    assert(!lasr_store_verify_saved(session.directory, 1));
    lasr_session_t new_session;
    assert(lasr_store_begin(&new_session, root, "five-dot", 1, 5000));
    assert(strcmp(new_session.directory, session.directory) != 0);
    const lasr_shot_t third = shot(1, 800, .25f, .3f, 5);
    assert(lasr_store_shot(&new_session, &third));
    char pending[LASR_SESSION_PATH_MAX];
    int n=snprintf(pending, sizeof(pending), "%s/session.pending", new_session.directory);
    assert(n > 0 && n < (int)sizeof(pending));
    lasr_store_abort(&new_session); // Simulated crash: recoverable marker stays.
    assert(new_session.phase == LASR_STORE_INCOMPLETE);
    assert(access(pending, F_OK) == 0);
    assert(!lasr_store_verify_saved(new_session.directory, 1));
    puts("LASR session store: pass (save/verify/rejection/recovery marker)");
    return 0;
}