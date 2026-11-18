#define _POSIX_C_SOURCE 200809L
#include "lasr_usb_wire.h"
#include "lasr_session_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    assert(lasr_usb_safe_session("session-000123"));
    assert(!lasr_usb_safe_session("session-0001/../"));
    assert(!lasr_usb_safe_session("../session-0001"));
    assert(lasr_usb_safe_file("shots.csv"));
    assert(!lasr_usb_safe_file("../../secrets"));
    assert((lasr_usb_crc32(0xffffffffu,"123456789",9)^0xffffffffu)==0xcbf43926u);
    char root[]="/tmp/lasr-usb-test-XXXXXX";
    assert(mkdtemp(root));
    lasr_session_t stored;
    assert(lasr_store_begin(&stored,root,"bullseye",1,3000));
    lasr_shot_t detected={.sequence=1,.elapsed_ms=750,.x_norm=.5f,.y_norm=.5f,.score=10};
    (void)snprintf(detected.zone,sizeof(detected.zone),"CENTER");
    assert(lasr_store_shot(&stored,&detected));
    assert(lasr_store_finish(&stored,800));
    const char *id=strrchr(stored.directory,'/');assert(id);id++;
    assert(lasr_usb_verified(root,id));
    char *body=NULL;size_t size=0;
    FILE *out=open_memstream(&body,&size);assert(out);
    assert(lasr_usb_send_file(out,root,id,"shots.csv"));
    assert(fclose(out)==0);
    assert(strstr(body,"!LASR|BEGIN|session-000001|shots.csv|"));
    assert(strstr(body,"!LASR|DATA|0|"));
    assert(strstr(body,"!LASR|END|"));
    free(body);
    puts("LASR USB wire: pass (validation, CRC32, framed export)");
    return 0;
}