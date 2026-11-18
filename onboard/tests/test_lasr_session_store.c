#define _POSIX_C_SOURCE 200809L
#include "../main/lasr_session_store.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void check_saved(const char *folder) {
    char path[300],line[512];
    snprintf(path,sizeof(path),"%s/session.json",folder);
    FILE *f=fopen(path,"r"); assert(f);
    assert(fgets(line,sizeof(line),f));
    assert(strstr(line,"\"shot_count\":3"));
    assert(strstr(line,"\"score_total\":17"));
    assert(fclose(f)==0);
    snprintf(path,sizeof(path),"%s/shots.csv",folder);
    f=fopen(path,"r"); assert(f);
    size_t n=0;
    while(fgets(line,sizeof(line),f))n++;
    assert(n==4);assert(fclose(f)==0);
}
int main(void) {
    char root[]="/tmp/lasr-storage-test-XXXXXX";
    assert(mkdtemp(root));
    lasr_session_store_t session={0};
    assert(!lasr_store_begin(&session,root,"../escape","bullseye",1,3000));
    assert(lasr_store_begin(&session,root,"session_001","bullseye",1,3000));
    const lasr_shot_t shots[]={
        {1,100,100,10,.5f,.5f,"R1"},
        {2,320,220,5,.2f,.6f,"R5"},
        {3,460,140,2,.2f,.7f,"OUTER"}
    };
    lasr_shot_t wrong=shots[0]; wrong.sequence=2;
    assert(!lasr_store_append(&session,&wrong));
    for(size_t i=0;i<3;i++)assert(lasr_store_append(&session,&shots[i]));
    assert(lasr_store_finish(&session));
    assert(session.state==LASR_STORE_SAVED);
    check_saved(session.folder);
    lasr_session_store_t again={0};
    assert(!lasr_store_begin(&again,root,"session_001","bullseye",1,3000));
    // Incomplete sessions retain a journal and must never be marked saved.
    lasr_session_store_t incomplete={0};
    assert(lasr_store_begin(&incomplete,root,"session_002","bullseye",2,3000));
    assert(lasr_store_append(&incomplete,&shots[0]));
    lasr_store_abort(&incomplete);
    assert(incomplete.state==LASR_STORE_ERROR);
    char pending[300];snprintf(pending,sizeof(pending),"%s/shots.pending",incomplete.folder);
    assert(access(pending,R_OK)==0);
    puts("LASR session store: 3 shots, verified outputs, path rejection, journal recovery passed");
    return 0;
}