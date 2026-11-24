#define _POSIX_C_SOURCE 200809L
#include "../main/lasr_sd_session_probe.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void check_file(const char *path, const char *expected) {
    char buffer[1024] = {0};
    FILE *file = fopen(path, "rb");
    assert(file && "Diagnostic output not created");
    const size_t len = fread(buffer, 1, sizeof(buffer)-1, file);
    assert(!ferror(file) && fclose(file) == 0);
    assert(len > 0 && strstr(buffer, expected));
}

int main(void) {
    char root[] = "/tmp/lasr-sd-session-probe-XXXXXX";
    assert(mkdtemp(root));
    assert(!lasr_sd_session_probe(root, "../escape"));
    assert(lasr_sd_session_probe(root, "A1"));
    char path[512];
    const char *folder = "LASR/diagnostics/synthetic-session-A1/LASR/sessions/fake_shots";
    assert(snprintf(path, sizeof(path), "%s/%s/shots.csv", root, folder) < (int)sizeof(path));
    check_file(path, "2,410,260,UNSCORED,0,0.750000,0.400000");
    assert(snprintf(path, sizeof(path), "%s/%s/session.json", root, folder) < (int)sizeof(path));
    check_file(path, "\"shot_count\":2");
    check_file(path, "\"save_state\":\"saved\"");
    assert(snprintf(path, sizeof(path), "%s/%s/shots.pending", root, folder) < (int)sizeof(path));
    check_file(path, "1,150,150,UNSCORED");
    // Reusing an ID is forbidden, preserving the first result byte-for-byte.
    assert(!lasr_sd_session_probe(root, "A1"));
    assert(lasr_sd_session_probe(root, "A2"));
    puts("SD session diagnostic: real writer, CSV/JSON, journal, no-overwrite PASS");
    return 0;
}