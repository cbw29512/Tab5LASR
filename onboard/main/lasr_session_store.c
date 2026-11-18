#include "lasr_session_store.h"
#include "lasr_session_fs.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool path_for(char *dest,size_t capacity,const char *folder,const char *file) {
    const int n=snprintf(dest,capacity,"%s/%s",folder,file);
    return n>0 && (size_t)n<capacity;
}
static bool fail(lasr_session_store_t *s) {
    if (s) {s->state=LASR_STORE_ERROR;s->last_errno=errno;}
    return false;
}

bool lasr_store_begin(lasr_session_store_t *s,const char *root,const char *id,
                      const char *target,uint32_t revision,uint32_t delay_ms) {
    if (!s || !root || !lasr_fs_id_valid(id) || !lasr_fs_id_valid(target)) return false;
    memset(s,0,sizeof(*s));
    char base[256], sessions[256], journal_path[300];
    if (!path_for(base,sizeof(base),root,"LASR") ||
        !path_for(sessions,sizeof(sessions),base,"sessions") ||
        !path_for(s->folder,sizeof(s->folder),sessions,id)) return fail(s);
    if (!lasr_fs_mkdir(base) || !lasr_fs_mkdir(sessions)) return fail(s);
    // Never reuse a session name; an existing directory may contain recovery data.
    struct stat previous;
    if (stat(s->folder,&previous)==0 || !lasr_fs_mkdir(s->folder))return fail(s);
    if (!path_for(journal_path,sizeof(journal_path),s->folder,"shots.pending"))return fail(s);
    s->journal=fopen(journal_path,"wb");
    if (!s->journal)return fail(s);
    snprintf(s->session_id,sizeof(s->session_id),"%s",id);
    snprintf(s->target_id,sizeof(s->target_id),"%s",target);
    s->calibration_revision=revision; s->delay_ms=delay_ms;
    if (fputs("shot,elapsed_ms,split_ms,zone,score,x_norm,y_norm\n",s->journal)<0 ||
        fflush(s->journal)!=0) {lasr_store_abort(s);return false;}
    s->state=LASR_STORE_RECORDING;
    return true;
}

bool lasr_store_append(lasr_session_store_t *s,const lasr_shot_t *shot) {
    if (!s || !shot || s->state!=LASR_STORE_RECORDING || !s->journal)return false;
    if (shot->sequence!=s->shots+1 || (s->shots&&shot->elapsed_ms<s->last_elapsed_ms) ||
        (s->shots&&shot->split_ms!=shot->elapsed_ms-s->last_elapsed_ms) ||
        (!s->shots&&shot->split_ms!=shot->elapsed_ms) ||
        !isfinite(shot->x_norm) || !isfinite(shot->y_norm) ||
        shot->x_norm<0 || shot->x_norm>1 || shot->y_norm<0 || shot->y_norm>1 ||
        !lasr_fs_id_valid(shot->zone))return false;
    if (fprintf(s->journal,"%lu,%lu,%lu,%s,%ld,%.6f,%.6f\n",
                (unsigned long)shot->sequence,(unsigned long)shot->elapsed_ms,
                (unsigned long)shot->split_ms,shot->zone,(long)shot->score,
                (double)shot->x_norm,(double)shot->y_norm)<0 || fflush(s->journal)!=0)
        return fail(s);
    // Immediate flush, not fsync per event; pending journal recovery is best effort.
    s->shots++;s->last_elapsed_ms=shot->elapsed_ms;s->total_score+=shot->score;
    return true;
}

bool lasr_store_finish(lasr_session_store_t *s) {
    if (!s || s->state!=LASR_STORE_RECORDING)return false;
    s->state=LASR_STORE_FINALIZING;
    if (!lasr_fs_sync_close(&s->journal))return fail(s);
    char pending[300],csv[300],meta[300],target_path[300],body[512];
    if (!path_for(pending,sizeof(pending),s->folder,"shots.pending") ||
        !path_for(csv,sizeof(csv),s->folder,"shots.csv") ||
        !path_for(meta,sizeof(meta),s->folder,"session.json") ||
        !path_for(target_path,sizeof(target_path),s->folder,"target.json"))return fail(s);
    if (!lasr_fs_copy_verified(pending,csv,(size_t)s->shots+1))return fail(s);
    snprintf(body,sizeof(body),"{\"schema_version\":1,\"target_id\":\"%s\","
             "\"calibration_revision\":%lu}\n",s->target_id,
             (unsigned long)s->calibration_revision);
    if (!lasr_fs_write_atomic(target_path,body))return fail(s);
    snprintf(body,sizeof(body),"{\"schema_version\":1,\"session_id\":\"%s\","
             "\"target_id\":\"%s\",\"status\":\"complete\","
             "\"shot_count\":%lu,\"score_total\":%lld,\"elapsed_ms\":%lu,"
             "\"start_delay_ms\":%lu,\"save_state\":\"saved\"}\n",
             s->session_id,s->target_id,(unsigned long)s->shots,
             (long long)s->total_score,(unsigned long)s->last_elapsed_ms,
             (unsigned long)s->delay_ms);
    if (!lasr_fs_write_atomic(meta,body))return fail(s);
    // Journal remains for crash recovery; caller must not delete on a guessed success.
    s->state=LASR_STORE_SAVED;
    return true;
}

void lasr_store_abort(lasr_session_store_t *s) {
    if (!s)return;
    if (s->journal) {fflush(s->journal);fclose(s->journal);s->journal=NULL;}
    s->state=LASR_STORE_ERROR;
}