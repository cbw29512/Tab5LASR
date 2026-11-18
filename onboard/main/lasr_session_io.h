#pragma once
#include "lasr_session_store.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool lasr_io_safe_id(const char *text, size_t max_len);
bool lasr_io_path(char *dst, size_t cap, const char *dir, const char *file);
bool lasr_io_mkdir(const char *dir);
bool lasr_io_sync_close(FILE **fp);
bool lasr_io_write_atomic(const char *path, const char *value);
bool lasr_io_write_target(lasr_session_t *s);
bool lasr_io_write_summary(lasr_session_t *s, uint64_t final_ms);
bool lasr_io_check_csv(const char *path, uint32_t expected_shots);
void lasr_io_error(lasr_session_t *s, const char *reason);