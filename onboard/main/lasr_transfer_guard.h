#pragma once
#include <stdbool.h>

typedef enum { LASR_MEDIA_LOCAL, LASR_MEDIA_PREPARE_USB,
               LASR_MEDIA_USB, LASR_MEDIA_EJECTING, LASR_MEDIA_ERROR } lasr_media_state_t;
typedef struct {
    lasr_media_state_t state;
    bool writer_idle, files_flushed, local_unmounted;
    bool usb_host_released;
} lasr_transfer_guard_t;

void lasr_transfer_guard_init(lasr_transfer_guard_t *guard);
bool lasr_transfer_request_usb(lasr_transfer_guard_t *guard);
bool lasr_transfer_set_unmounted(lasr_transfer_guard_t *guard);
bool lasr_transfer_usb_ready(lasr_transfer_guard_t *guard);
bool lasr_transfer_request_eject(lasr_transfer_guard_t *guard);
bool lasr_transfer_host_released(lasr_transfer_guard_t *guard);
bool lasr_transfer_local_mounted(lasr_transfer_guard_t *guard);
bool lasr_transfer_can_write(const lasr_transfer_guard_t *guard);