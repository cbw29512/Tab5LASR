#include "lasr_transfer_guard.h"
#include <stddef.h>

void lasr_transfer_guard_init(lasr_transfer_guard_t *g) {
    if (!g)return;
    *g=(lasr_transfer_guard_t){.state=LASR_MEDIA_LOCAL};
}
bool lasr_transfer_request_usb(lasr_transfer_guard_t *g) {
    if (!g||g->state!=LASR_MEDIA_LOCAL||!g->writer_idle||!g->files_flushed)return false;
    g->state=LASR_MEDIA_PREPARE_USB;return true;
}
bool lasr_transfer_set_unmounted(lasr_transfer_guard_t *g) {
    if (!g||g->state!=LASR_MEDIA_PREPARE_USB||!g->writer_idle||!g->files_flushed)return false;
    g->local_unmounted=true;return true;
}
bool lasr_transfer_usb_ready(lasr_transfer_guard_t *g) {
    if (!g||g->state!=LASR_MEDIA_PREPARE_USB||!g->local_unmounted)return false;
    g->state=LASR_MEDIA_USB;return true;
}
bool lasr_transfer_request_eject(lasr_transfer_guard_t *g) {
    if (!g||g->state!=LASR_MEDIA_USB)return false;
    g->state=LASR_MEDIA_EJECTING;return true;
}
bool lasr_transfer_host_released(lasr_transfer_guard_t *g) {
    if (!g||g->state!=LASR_MEDIA_EJECTING)return false;
    g->usb_host_released=true;return true;
}
bool lasr_transfer_local_mounted(lasr_transfer_guard_t *g) {
    if (!g||g->state!=LASR_MEDIA_EJECTING||!g->usb_host_released)return false;
    g->local_unmounted=false;g->usb_host_released=false;
    g->state=LASR_MEDIA_LOCAL;return true;
}
bool lasr_transfer_can_write(const lasr_transfer_guard_t *g) {
    return g && g->state==LASR_MEDIA_LOCAL && !g->local_unmounted;
}