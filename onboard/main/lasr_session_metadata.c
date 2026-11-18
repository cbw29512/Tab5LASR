#include "lasr_session_io.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

bool lasr_io_write_target(lasr_session_t *s) {
    if (!s) return false;
    char path[LASR_SESSION_PATH_MAX], value[256];
    if (!lasr_io_path(path, sizeof(path), s->directory, "target.json")) return false;
    int n = snprintf(value, sizeof(value),
        "{\"schema_version\":1,\"template_id\":\"%s\","
        "\"calibration_revision\":%" PRIu32 "}\n",
        s->target, s->calibration_revision);
    return n > 0 && (size_t)n < sizeof(value) && lasr_io_write_atomic(path, value);
}

bool lasr_io_write_summary(lasr_session_t *s, uint64_t final_ms) {
    if (!s) return false;
    char path[LASR_SESSION_PATH_MAX], value[512];
    if (!lasr_io_path(path, sizeof(path), s->directory, "session.json")) return false;
    const int n = snprintf(value, sizeof(value),
        "{\"schema_version\":1,\"status\":\"complete\","
        "\"template_id\":\"%s\",\"calibration_revision\":%" PRIu32 ","
        "\"start_delay_ms\":%" PRIu32 ",\"elapsed_ms\":%" PRIu64 ","
        "\"shot_count\":%" PRIu32 ",\"score_total\":%" PRId64 "}\n",
        s->target, s->calibration_revision, s->delay_ms, final_ms,
        s->shot_count, s->score_total);
    return n > 0 && (size_t)n < sizeof(value) && lasr_io_write_atomic(path, value);
}

bool lasr_io_check_csv(const char *path, uint32_t expected_shots) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    char line[180];
    uint32_t lines = 0;
    bool valid = true;
    if (!fgets(line, sizeof(line), fp) ||
        strcmp(line, "shot,elapsed_ms,split_ms,x_norm,y_norm,zone,score\n") != 0)
        valid = false;
    while (valid && fgets(line, sizeof(line), fp)) {
        if (!strchr(line, '\n') || ++lines > expected_shots) valid = false;
    }
    if (ferror(fp)) valid = false;
    if (fclose(fp) != 0) valid = false;
    return valid && lines == expected_shots;
}