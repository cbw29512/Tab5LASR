#pragma once
#include <stdbool.h>

// Writes and verifies explicitly synthetic diagnostic shot events.
// Never writes inside /LASR/sessions; leaves diagnostic files for inspection.
bool lasr_sd_session_probe(const char *sd_root, const char *unique_id);