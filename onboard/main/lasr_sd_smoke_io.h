#pragma once
#include <stdbool.h>

// Portable file I/O only: caller supplies an ALREADY mounted filesystem root.
// Writes exactly one unique diagnostics file, verifies its bytes, then removes it.
// Existing files are NEVER overwritten or removed by this function.
bool lasr_sd_smoke_file(const char *root, const char *unique_id);