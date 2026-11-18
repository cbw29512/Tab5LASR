#pragma once
#include <stdbool.h>

// Read-only verification of known diagnostic session after SD unmount/remount.
// NEVER treat the synthetic contents as real camera events.
bool lasr_sd_session_verify(const char *sd_root, const char *unique_id);