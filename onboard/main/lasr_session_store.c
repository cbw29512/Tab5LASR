#include "lasr_session_io.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool lasr_store_begin(lasr_session_t *s, const char *root, const char *target,
                      uint32_t revision, uint32_t delay_ms) {
    if (!s || !root || !target || !lasr_io_safe_id(target, LASR_TARGET_MAX - 1)) return false;
    memset(s, 0, sizeof(*s));
    char sessions[LASR_SESSION_PATH_MAX], parent[LASR_SESSION_PATH_MAX];
    if (!lasr_io_path(parent, sizeof(parent), root, "LASR") ||
        !lasr_io_path(sessions, sizeof(sessions), parent, "sessions") ||
        !lasr_io_mkdir(parent) || !lasr_io_mkdir(sessions)) goto failed;
    bool created = false;
    for (unsigned id = 1; id <= 999999; ++id) {
        char name[32];
        (void)snprintf(name, sizeof(name), "session-%06u", id);
        if (!lasr_io_path(s->directory, sizeof(s->directory), sessions, name)) goto failed;
        if (mkdir(s->directory, 0755) == 0) { created = true; break; }
        if (errno != EEXIST) goto failed;
    }
    if (!created) goto failed;
    (void)snprintf(s->target, sizeof(s->target), "%s", target);
    s->calibration_revision = revision;
    s->delay_ms = delay_ms;
    char pending[LASR_SESSION_PATH_MAX], journal[LASR_SESSION_PATH_MAX];
    if (!lasr_io_path(pending, sizeof(pending), s->directory, "session.pending") ||
        !lasr_io_path(journal, sizeof(journal), s->directory, "shots.pending.csv") ||
        !lasr_io_write_atomic(pending, "incomplete\n") || !lasr_io_write_target(s)) goto failed;
    s->journal = fopen(journal, "wb");
    if (!s->journal || fputs("shot,elapsed_ms,split_ms,x_norm,y_norm,zone,score\n", s->journal) < 0 ||
        fflush(s->journal) != 0 || fsync(fileno(s->journal)) != 0) goto failed;
    s->phase = LASR_STORE_RECORDING;
    return true;
failed:
    lasr_io_error(s, "Unable to create recoverable session");
    if (s->journal) { fclose(s->journal); s->journal = NULL; }
    return false;
}

bool lasr_store_shot(lasr_session_t *s, const lasr_shot_t *shot) {
    if (!s || !shot || s->phase != LASR_STORE_RECORDING || !s->journal) return false;
    if (shot->sequence != s->shot_count + 1 ||
        (s->shot_count && shot->elapsed_ms < s->last_elapsed_ms) ||
        !isfinite(shot->x_norm) || !isfinite(shot->y_norm) ||
        shot->x_norm < 0 || shot->x_norm > 1 || shot->y_norm < 0 || shot->y_norm > 1 ||
        !lasr_io_safe_id(shot->zone, sizeof(shot->zone) - 1)) return false;
    const uint64_t split = shot->elapsed_ms - s->last_elapsed_ms;
    if (fprintf(s->journal, "%" PRIu32 ",%" PRIu64 ",%" PRIu64 ",%.6f,%.6f,%s,%" PRId32 "\n",
                shot->sequence, shot->elapsed_ms, split, shot->x_norm, shot->y_norm,
                shot->zone, shot->score) < 0 ||
        fflush(s->journal) != 0 || fsync(fileno(s->journal)) != 0) {
        lasr_io_error(s, "Unable to persist shot journal");
        return false;
    }
    s->last_elapsed_ms = shot->elapsed_ms;
    s->score_total += shot->score;
    s->shot_count++;
    return true;
}