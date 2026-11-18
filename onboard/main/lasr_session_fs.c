#include "lasr_session_fs.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool lasr_fs_id_valid(const char *id) {
    if (!id || !id[0]) return false;
    size_t n = strlen(id);
    if (n > 40) return false;
    for (size_t i=0;i<n;i++) {
        const unsigned char c=(unsigned char)id[i];
        if (!(isalnum(c) || c=='-' || c=='_')) return false;
    }
    return true;
}

bool lasr_fs_mkdir(const char *path) {
    if (!path || !path[0]) return false;
    return mkdir(path,0775)==0 || errno==EEXIST;
}

bool lasr_fs_sync_close(FILE **file) {
    if (!file || !*file) return false;
    FILE *f=*file; *file=NULL;
    bool ok=fflush(f)==0;
    if (ok) ok=fsync(fileno(f))==0;
    if (fclose(f)!=0) ok=false;
    return ok;
}

bool lasr_fs_copy_verified(const char *source, const char *dest, size_t header_rows) {
    FILE *in=fopen(source,"rb"), *out=NULL;
    if (!in) return false;
    out=fopen(dest,"wb");
    if (!out) { fclose(in); return false; }
    char buf[256]; size_t lines=0, read_bytes=0;
    bool ok=true;
    while (fgets(buf,sizeof(buf),in)) {
        const size_t len=strlen(buf);
        if (len==0 || buf[len-1]!='\n' || fwrite(buf,1,len,out)!=len) {ok=false;break;}
        lines++; read_bytes+=len;
    }
    if (ferror(in) || fclose(in)!=0) ok=false;
    if (!lasr_fs_sync_close(&out)) ok=false;
    if (!ok || lines!=header_rows || !read_bytes) {remove(dest);return false;}
    FILE *verify=fopen(dest,"rb");
    if (!verify) return false;
    while (fread(buf,1,sizeof(buf),verify)>0) { /* ensure file is readable */ }
    if (ferror(verify)) ok=false;
    if (fclose(verify)!=0) ok=false;
    // Verify exact byte count, including final short read, via stat (not rounded reads).
    struct stat st;
    if (stat(dest,&st)!=0 || (uint64_t)st.st_size!=read_bytes) ok=false;
    if (!ok) remove(dest);
    return ok;
}

bool lasr_fs_write_atomic(const char *path, const char *text) {
    char tmp[300];
    if (!path || !text || snprintf(tmp,sizeof(tmp),"%s.part",path)>=(int)sizeof(tmp))return false;
    FILE *f=fopen(tmp,"wb");
    if (!f)return false;
    const size_t n=strlen(text);
    bool ok=fwrite(text,1,n,f)==n;
    if (!lasr_fs_sync_close(&f))ok=false;
    if (!ok || rename(tmp,path)!=0) {remove(tmp);return false;}
    struct stat st;
    return stat(path,&st)==0 && st.st_size==(off_t)n;
}