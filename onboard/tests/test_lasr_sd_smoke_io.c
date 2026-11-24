#define _POSIX_C_SOURCE 200809L
#include "../main/lasr_sd_smoke_io.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
    char dir[]="/tmp/lasr-sd-smoke-XXXXXX";
    assert(mkdtemp(dir));
    assert(!lasr_sd_smoke_file(NULL,"one"));
    assert(!lasr_sd_smoke_file(dir,"../escape"));
    assert(lasr_sd_smoke_file(dir,"one"));
    char path[300];
    snprintf(path,sizeof(path),"%s/LASR/diagnostics/lasr-smoke-one.txt",dir);
    assert(access(path,F_OK)!=0); // Removed only our verified diagnostic.
    const int file=open(path,O_CREAT|O_EXCL|O_WRONLY,0600);
    assert(file>=0);
    const char original[]="USER FILE DO NOT OVERWRITE";
    assert(write(file,original,sizeof(original))==(ssize_t)sizeof(original));
    assert(close(file)==0);
    assert(!lasr_sd_smoke_file(dir,"one")); // O_EXCL protects existing file.
    char readback[64]={0};
    FILE *f=fopen(path,"rb");assert(f);
    assert(fread(readback,1,sizeof(original),f)==sizeof(original));
    assert(fclose(f)==0 && memcmp(readback,original,sizeof(original))==0);
    puts("LASR microSD smoke IO: unique file, readback, cleanup, no overwrite PASS");
}