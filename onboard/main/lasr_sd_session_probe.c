#include "lasr_sd_session_probe.h"
#include "lasr_session_fs.h"
#include "lasr_session_store.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool append_part(char *out, size_t capacity,
                        const char *left, const char *right) {
    const int n = snprintf(out, capacity, "%s/%s", left, right);
    return n > 0 && (size_t)n < capacity;
}

bool lasr_sd_session_probe(const char *root, const char *id) {
    if (!root || !*root || !lasr_fs_id_valid(id)) {
        errno = EINVAL;
        return false;
    }
    char base[256], diagnostics[256], name[64], isolated[256];
    if (!append_part(base, sizeof(base), root, "LASR") ||
        !append_part(diagnostics, sizeof(diagnostics), base, "diagnostics") ||
        !lasr_fs_mkdir(base) || !lasr_fs_mkdir(diagnostics)) return false;
    const int n = snprintf(name, sizeof(name), "synthetic-session-%s", id);
    if (n <= 0 || (size_t)n >= sizeof(name) ||
        !append_part(isolated, sizeof(isolated), diagnostics, name)) {
        errno = ENAMETOOLONG;
        return false;
    }
    // Exclusive directory creation prevents overwriting an old diagnostic.
    if (mkdir(isolated, 0775) != 0) return false;

    lasr_session_store_t s = {0};
    if (!lasr_store_begin(&s, isolated, "fake_shots", "synthetic_test", 1, 3000)) {
        lasr_store_abort(&s);
        return false;
    }
    // These are sample events, NOT laser observations or actual scores.
    const lasr_shot_t first = {
        .sequence=1, .elapsed_ms=150, .split_ms=150,
        .score=0, .x_norm=0.25f, .y_norm=0.50f, .zone="UNSCORED"
    };
    const lasr_shot_t second = {
        .sequence=2, .elapsed_ms=410, .split_ms=260,
        .score=0, .x_norm=0.75f, .y_norm=0.40f, .zone="UNSCORED"
    };
    if (!lasr_store_append(&s, &first) || !lasr_store_append(&s, &second) ||
        !lasr_store_finish(&s)) {
        lasr_store_abort(&s); // Keep the journal on SD for investigation.
        return false;
    }
    return s.state == LASR_STORE_SAVED;
}