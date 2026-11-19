#include "lasr_usb_preview_wire.h"
#include <stdio.h>

static uint32_t hash_jpeg(const uint8_t *buf, size_t len) {
    uint32_t hash=2166136261u; // FNV-1a, not a cryptographic signature
    for (size_t i=0;i<len;i++) hash=(hash^buf[i])*16777619u;
    return hash;
}

size_t lasr_usb_frame_format(const uint8_t *jpeg, size_t length,
                             uint32_t sequence, uint32_t width, uint32_t height,
                             char *destination, size_t capacity) {
    static const char alphabet[]=
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (!jpeg || length<4 || !destination || !capacity ||
        length>65536 || !width || !height ||
        jpeg[0]!=0xff || jpeg[1]!=0xd8 ||
        jpeg[length-2]!=0xff || jpeg[length-1]!=0xd9)
        return 0;
    const int prefix=snprintf(destination,capacity,
        "LASRJPEGV1 %lu %lu %lu %lu %08lx ",
        (unsigned long)sequence,(unsigned long)width,
        (unsigned long)height,(unsigned long)length,
        (unsigned long)hash_jpeg(jpeg,length));
    if (prefix<0 || (size_t)prefix>=capacity) return 0;
    const size_t encoded=4*((length+2)/3);
    if (capacity-(size_t)prefix<=encoded) return 0;
    size_t k=(size_t)prefix;
    for (size_t i=0;i<length;i+=3) {
        const uint32_t a=jpeg[i],b=(i+1<length?jpeg[i+1]:0),
            c=(i+2<length?jpeg[i+2]:0);
        destination[k++]=alphabet[a>>2];
        destination[k++]=alphabet[((a&3u)<<4)|(b>>4)];
        destination[k++]=i+1<length?alphabet[((b&15u)<<2)|(c>>6)]:'=';
        destination[k++]=i+2<length?alphabet[c&63u]:'=';
    }
    destination[k++]='\n';
    destination[k]='\0';
    return k;
}
