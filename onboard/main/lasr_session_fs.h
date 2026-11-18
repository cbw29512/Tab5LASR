#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

bool lasr_fs_id_valid(const char *id);
bool lasr_fs_mkdir(const char *path);
bool lasr_fs_sync_close(FILE **file);
bool lasr_fs_copy_verified(const char *source, const char *dest, size_t header_rows);
bool lasr_fs_write_atomic(const char *path, const char *text);