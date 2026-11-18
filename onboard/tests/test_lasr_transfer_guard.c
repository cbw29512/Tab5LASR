#include "../main/lasr_transfer_guard.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    lasr_transfer_guard_t g;
    lasr_transfer_guard_init(&g);
    assert(lasr_transfer_can_write(&g));
    assert(!lasr_transfer_request_usb(&g));
    g.writer_idle=true;g.files_flushed=true;
    assert(lasr_transfer_request_usb(&g));
    assert(!lasr_transfer_can_write(&g));
    assert(!lasr_transfer_usb_ready(&g));
    assert(lasr_transfer_set_unmounted(&g));
    assert(lasr_transfer_usb_ready(&g));
    assert(!lasr_transfer_local_mounted(&g));
    assert(!lasr_transfer_can_write(&g));
    assert(lasr_transfer_request_eject(&g));
    assert(!lasr_transfer_local_mounted(&g));
    assert(lasr_transfer_host_released(&g));
    assert(lasr_transfer_local_mounted(&g));
    assert(lasr_transfer_can_write(&g));
    puts("LASR transfer guard: exclusive media ownership and eject gate passed");
    return 0;
}