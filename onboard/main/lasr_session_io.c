#include "lasr_session_io.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool lasr_io_safe_id(const char *text, size_t max_len) {
    if (!text) return false;
    const size_t len = strnlen(text, max_len + 1);
    if (!len || len > max_len) return false;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '-' || c == '_')) return false;
    }
    return true;
}

bool lasr_io_path(char *dst, size_t cap, const char *dir, const char *file) {
    if (!dst || !dir || !file || !cap) return false;
    const int n = snprintf(dst, cap, "%s/%s", dir, file);
    return n > 0 && (size_t)n < cap;
}

bool lasr_io_mkdir(const char *dir) {
    if (mkdir(dir, 0755) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat st;
    return stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
}

bool lasr_io_sync_close(FILE **fp) {
    if (!fp || !*fp) return false;
    FILE *file = *fp;
    *fp = NULL;
    const bool flushed = fflush(file) == 0;
    const bool synced = flushed && fsync(fileno(file)) == 0;
    const bool closed = fclose(file) == 0;
    return flushed && synced && closed;
}

bool lasr_io_write_atomic(const char *path, const char *value) {
    if (!path || !value) return false;
    char temp[LASR_SESSION_PATH_MAX + 16];
    if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) return false;
    FILE *fp = fopen(temp, "wb");
    if (!fp) return false;
    const size_t size = strlen(value);
    if (fwrite(value, 1, size, fp) != size || !lasr_io_sync_close(&fp)) {
        if (fp) fclose(fp);
        remove(temp);
        return false;
    }
    // FAT rename is not a power-failure transaction; the pending journal remains.
    if (rename(temp, path) != 0) { remove(temp); return false; }
    return true;
}

void lasr_io_error(lasr_session_t *s, const char *reason) {
    if (!s) return;
    s->phase = LASR_STORE_ERROR;
    (void)snprintf(s->last_error, sizeof(s->last_error), "%s (errno=%d)",
                   reason ? reason : "Storage error", errno);
}