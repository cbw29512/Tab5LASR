#include "lasr_sd_session_verify.h"
#include "lasr_session_fs.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

// The intentionally fixed fixture makes corruption and extra rows detectable.
static const char k_shots[] =
    "shot,elapsed_ms,split_ms,zone,score,x_norm,y_norm\n"
    "1,150,150,UNSCORED,0,0.250000,0.500000\n"
    "2,410,260,UNSCORED,0,0.750000,0.400000\n";
static const char k_target[] =
    "{\"schema_version\":1,\"target_id\":\"synthetic_test\","
    "\"calibration_revision\":1}\n";
static const char k_session[] =
    "{\"schema_version\":1,\"session_id\":\"fake_shots\","
    "\"target_id\":\"synthetic_test\",\"status\":\"complete\","
    "\"shot_count\":2,\"score_total\":0,\"elapsed_ms\":410,"
    "\"start_delay_ms\":3000,\"save_state\":\"saved\"}\n";

// Read exact bytes and EOF, including all text metadata; never modify files.
static bool exact_file(const char *path, const char *expected) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    const size_t length = strlen(expected);
    char buf[256] = {0};
    const bool valid_length = length < sizeof(buf);
    bool equal = valid_length && fread(buf, 1, length, file) == length;
    if (equal) equal = memcmp(buf, expected, length) == 0 && fgetc(file) == EOF;
    const bool io_error = ferror(file) != 0;
    const int close_rc = fclose(file);
    if (io_error || close_rc != 0) equal = false;
    return equal;
}

bool lasr_sd_session_verify(const char *root, const char *id) {
    if (!root || !*root || !lasr_fs_id_valid(id)) {
        errno = EINVAL;
        return false;
    }
    char base[320];
    const int n = snprintf(base, sizeof(base),
        "%s/LASR/diagnostics/synthetic-session-%s/LASR/sessions/fake_shots", root, id);
    if (n <= 0 || (size_t)n >= sizeof(base)) {
        errno = ENAMETOOLONG;
        return false;
    }
    struct { const char *name; const char *expected; } files[] = {
        {"shots.pending", k_shots}, {"shots.csv", k_shots},
        {"target.json", k_target}, {"session.json", k_session}
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char path[352];
        const int written = snprintf(path, sizeof(path), "%s/%s", base, files[i].name);
        if (written <= 0 || (size_t)written >= sizeof(path) ||
            !exact_file(path, files[i].expected)) return false;
    }
    return true;
}