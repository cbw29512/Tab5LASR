#include "lasr_usb_wire.h"
#include "lasr_session_store.h"
#include <stdio.h>
#include <string.h>

bool lasr_usb_safe_session(const char *id) {
    if (!id || strlen(id)!=14 || strncmp(id,"session-",8)!=0)return false;
    for(size_t i=8;i<14;i++) if(id[i]<'0'||id[i]>'9')return false;
    return true;
}

bool lasr_usb_safe_file(const char *name) {
    return name && (strcmp(name,"shots.csv")==0 ||
       strcmp(name,"session.json")==0 || strcmp(name,"target.json")==0);
}


bool lasr_usb_verified(const char *root, const char *id) {
    if(!root||!lasr_usb_safe_session(id))return false;
    char dir[LASR_SESSION_PATH_MAX],json[LASR_SESSION_PATH_MAX];
    int n=snprintf(dir,sizeof(dir),"%s/LASR/sessions/%s",root,id);
    if(n<1||(size_t)n>=sizeof(dir))return false;
    n=snprintf(json,sizeof(json),"%s/session.json",dir);
    if(n<1||(size_t)n>=sizeof(json))return false;
    FILE *fp=fopen(json,"rb");if(!fp)return false;
    char body[512]={0};size_t bytes=fread(body,1,sizeof(body)-1,fp);
    bool ok=!ferror(fp)&&fclose(fp)==0&&bytes>0;
    if(!ok)return false;
    const char *at=strstr(body,"\"shot_count\":");
    unsigned count=0;
    if(!at||sscanf(at,"\"shot_count\":%u",&count)!=1)return false;
    return lasr_store_verify_saved(dir,count);
}

uint32_t lasr_usb_crc32(uint32_t state, const void *data, size_t bytes) {
    const unsigned char *p=data;
    uint32_t crc=state;
    for(size_t i=0;i<bytes;i++) {
        crc^=p[i];
        for(unsigned bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
    }
    return crc;
}

bool lasr_usb_send_file(FILE *out, const char *root,
                        const char *id, const char *name) {
    if(!out||!root||!lasr_usb_safe_file(name)||!lasr_usb_verified(root,id))return false;
    char path[256];
    int n=snprintf(path,sizeof(path),"%s/LASR/sessions/%s/%s",root,id,name);
    if(n<1||(size_t)n>=sizeof(path))return false;
    FILE *fp=fopen(path,"rb");
    if(!fp)return false;
    if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return false;}
    long size=ftell(fp);
    if(size<0||size>10*1024*1024||fseek(fp,0,SEEK_SET)!=0){fclose(fp);return false;}
    // Host rejects incomplete transfer or incorrect CRC; no binary terminal data.
    if(fprintf(out,"!LASR|BEGIN|%s|%s|%ld\n",id,name,size)<0){fclose(fp);return false;}
    uint8_t block[48];uint32_t crc=0xffffffffu,seq=0;
    size_t read=0;
    while((read=fread(block,1,sizeof(block),fp))>0) {
        crc=lasr_usb_crc32(crc,block,read);
        if(fprintf(out,"!LASR|DATA|%lu|",(unsigned long)seq++)<0){fclose(fp);return false;}
        for(size_t i=0;i<read;i++) if(fprintf(out,"%02x",block[i])<0){fclose(fp);return false;}
        if(fputc('\n',out)==EOF){fclose(fp);return false;}
    }
    bool ok=!ferror(fp) && fclose(fp)==0;
    if(!ok)return false;
    return fprintf(out,"!LASR|END|%08lx\n",(unsigned long)(crc^0xffffffffu))>0 &&
           fflush(out)==0;
}