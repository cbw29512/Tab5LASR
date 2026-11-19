#include "../main/lasr_usb_preview_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// No hardware, library, simulated shots or camera frames involved.
// This verifies only bounded framing of an arbitrary JPEG-shaped test vector.
int main(void) {
    const uint8_t jpeg[]={0xff,0xd8,0,0xff,0xd9};
    char line[256]={0};
    const size_t n=lasr_usb_frame_format(jpeg,sizeof(jpeg),8,320,176,
                                          line,sizeof(line));
    assert(n>40 && line[n-1]=='\n' && line[n]=='\0');
    const char *prefix="LASRJPEGV1 8 320 176 5 ";
    assert(strncmp(line,prefix,strlen(prefix))==0);
    assert(strstr(line,"/9gA/9k=")!=NULL);
    assert(lasr_usb_frame_format(jpeg,sizeof(jpeg),8,320,176,line,10)==0);
    assert(lasr_usb_frame_format(jpeg,sizeof(jpeg),8,320,176,NULL,0)==0);
    const uint8_t broken[]={0x80,0xd8,0xff,0xd9};
    assert(lasr_usb_frame_format(broken,sizeof(broken),8,320,176,
                                 line,sizeof(line))==0);
    assert(lasr_usb_frame_format(jpeg,sizeof(jpeg),8,0,176,
                                 line,sizeof(line))==0);
    puts("LASR USB JPEG wire format: bounds, headers, framing and JPEG markers passed");
}
