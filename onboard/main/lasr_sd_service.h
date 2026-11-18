#pragma once
#include "lasr_session_store.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LASR_SD_NOT_STARTED=0, LASR_SD_MOUNTING, LASR_SD_NO_CARD,
    LASR_SD_READY, LASR_SD_RECORDING, LASR_SD_SAVED,
    LASR_SD_INCOMPLETE, LASR_SD_ERROR, LASR_SD_TEST_OK
} lasr_sd_status_t;

// An SD worker owns every file operation. Camera callbacks only queue events.
bool lasr_sd_service_init(void);
bool lasr_sd_service_begin(const char *template_id, uint32_t revision, uint32_t delay_ms);
bool lasr_sd_service_shot(const lasr_shot_t *shot);
bool lasr_sd_service_finish(uint64_t final_elapsed_ms);
bool lasr_sd_service_abort(void);
bool lasr_sd_service_probe(void); // User-initiated, NOT a shooting session.
bool lasr_sd_service_retry(void);
lasr_sd_status_t lasr_sd_service_status(void);