#include "lasr_sd_smoke_io.h"
#include "lasr_session_fs.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool lasr_sd_smoke_file(const char *root,const char *id) {
    if (!root || !root[0] || !lasr_fs_id_valid(id)) return false;
    char base[256],dir[256],path[300];
    const int n=snprintf(base,sizeof(base),"%s/LASR",root);
    if (n<1 || (size_t)n>=sizeof(base) || !lasr_fs_mkdir(base))return false;
    const int d=snprintf(dir,sizeof(dir),"%s/diagnostics",base);
    if (d<1 || (size_t)d>=sizeof(dir) || !lasr_fs_mkdir(dir))return false;
    const int p=snprintf(path,sizeof(path),"%s/lasr-smoke-%s.txt",dir,id);
    if (p<1 || (size_t)p>=sizeof(path))return false;
    // An existing file is never opened/truncated. The SD test is one owner.
    const int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0644);
    if (fd<0)return false;
    const char *expected="TAB5 LASR MICROSD READBACK TEST v1\n";
    const size_t bytes=strlen(expected);
    bool ok=write(fd,expected,bytes)==(ssize_t)bytes;
    if (ok)ok=fsync(fd)==0;
    if (close(fd)!=0)ok=false;
    if (!ok)return false; // Retain incomplete file for diagnostics.
    const int input=open(path,O_RDONLY);
    if (input<0)return false;
    char readback[64]={0};
    const ssize_t got=read(input,readback,sizeof(readback));
    ok=got==(ssize_t)bytes && memcmp(readback,expected,bytes)==0;
    if (close(input)!=0)ok=false;
    if (!ok)return false; // Never claim verification on corruption.
    // Remove only a file created by this invocation, after successful readback.
    return unlink(path)==0;
}