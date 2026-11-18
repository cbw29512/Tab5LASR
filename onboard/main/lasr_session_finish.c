#include "lasr_session_io.h"
#include <stdio.h>
#include <string.h>

bool lasr_store_verify_saved(const char *dir, uint32_t expected_shots) {
    char json[LASR_SESSION_PATH_MAX], csv[LASR_SESSION_PATH_MAX];
    char target[LASR_SESSION_PATH_MAX], pending[LASR_SESSION_PATH_MAX];
    if (!lasr_io_path(json, sizeof(json), dir, "session.json") ||
        !lasr_io_path(csv, sizeof(csv), dir, "shots.csv") ||
        !lasr_io_path(target, sizeof(target), dir, "target.json") ||
        !lasr_io_path(pending, sizeof(pending), dir, "session.pending")) return false;
    FILE *fp = fopen(json, "rb");
    if (!fp) return false;
    char buffer[512];
    size_t count = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[count] = '\0';
    bool ok = fclose(fp) == 0 &&
        strstr(buffer, "\"status\":\"complete\"") != NULL &&
        strstr(buffer, "\"schema_version\":1") != NULL;
    if (!ok) return false;
    fp = fopen(target, "rb");
    if (!fp) return false;
    const int first = fgetc(fp);
    const int closed = fclose(fp);
    ok = first == '{' && closed == 0;
    if (!ok || !lasr_io_check_csv(csv, expected_shots)) return false;
    fp = fopen(pending, "rb");
    if (fp) { fclose(fp); return false; }
    return true;
}

bool lasr_store_finish(lasr_session_t *s, uint64_t total_elapsed_ms) {
    if (!s || s->phase != LASR_STORE_RECORDING ||
        total_elapsed_ms < s->last_elapsed_ms) return false;
    if (!lasr_io_sync_close(&s->journal)) goto failed;
    char journal[LASR_SESSION_PATH_MAX], csv[LASR_SESSION_PATH_MAX];
    char pending[LASR_SESSION_PATH_MAX];
    if (!lasr_io_path(journal, sizeof(journal), s->directory, "shots.pending.csv") ||
        !lasr_io_path(csv, sizeof(csv), s->directory, "shots.csv") ||
        !lasr_io_path(pending, sizeof(pending), s->directory, "session.pending")) goto failed;
    if (!lasr_io_check_csv(journal, s->shot_count)) goto failed;
    if (rename(journal, csv) != 0) goto failed;
    if (!lasr_io_write_summary(s, total_elapsed_ms)) goto failed;
    // Keep the recovery marker until all files have been verified.
    if (!lasr_io_check_csv(csv, s->shot_count)) goto failed;
    if (remove(pending) != 0) goto failed;
    if (!lasr_store_verify_saved(s->directory, s->shot_count)) goto failed;
    s->phase = LASR_STORE_SAVED;
    return true;
failed:
    lasr_io_error(s, "Could not finalize verified session");
    return false;
}

void lasr_store_abort(lasr_session_t *s) {
    if (!s) return;
    if (s->journal) {
        (void)lasr_io_sync_close(&s->journal);
    }
    if (s->phase == LASR_STORE_RECORDING) s->phase = LASR_STORE_INCOMPLETE;
    // Do NOT remove .pending: a forced shutdown must remain discoverable.
}