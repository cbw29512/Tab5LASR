#define _POSIX_C_SOURCE 200809L
#include "../main/lasr_sd_session_probe.h"
#include "../main/lasr_sd_session_verify.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char root[] = "/tmp/lasr-sd-remount-XXXXXX";
    assert(mkdtemp(root) != NULL);
    assert(!lasr_sd_session_verify(root, "A1")); // Missing card data fails.
    assert(!lasr_sd_session_verify(root, "../bad")); // Reject traversal.
    assert(lasr_sd_session_probe(root, "A1"));
    // Simulates release + reopen via independent read-only verifier.
    assert(lasr_sd_session_verify(root, "A1"));
    char path[512];
    int n = snprintf(path, sizeof(path), "%s/LASR/diagnostics/"
                     "synthetic-session-A1/LASR/sessions/fake_shots/shots.csv", root);
    assert(n > 0 && (size_t)n < sizeof(path));
    FILE *file = fopen(path, "ab");
    assert(file);
    assert(fputs("TAMPER\n", file) >= 0 && fclose(file) == 0);
    assert(!lasr_sd_session_verify(root, "A1")); // Extra data fails.
    assert(lasr_sd_session_probe(root, "A2"));
    assert(lasr_sd_session_verify(root, "A2"));
    // Remove a mandatory metadata file: must not report PASS.
    n = snprintf(path, sizeof(path), "%s/LASR/diagnostics/"
        "synthetic-session-A2/LASR/sessions/fake_shots/session.json", root);
    assert(n > 0 && (size_t)n < sizeof(path));
    assert(unlink(path) == 0 && !lasr_sd_session_verify(root, "A2"));
    puts("LASR persistence verifier: exact bytes, no-overwrite, missing/tamper PASS");
    return 0;
}