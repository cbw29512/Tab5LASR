#include "lasr_sd_service.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL) && CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
#error "LASR must never automatically format an inserted microSD card"
#endif

static const char *TAG="lasr_sd";
typedef enum { CMD_BEGIN, CMD_SHOT, CMD_FINISH, CMD_ABORT, CMD_TEST, CMD_RETRY } cmd_type_t;
typedef struct {
    cmd_type_t type;
    char target[LASR_TARGET_MAX];
    uint32_t revision, delay_ms;
    lasr_shot_t shot;
    uint64_t final_elapsed_ms;
} cmd_t;
static QueueHandle_t s_queue;
static atomic_int s_state;
static atomic_bool s_drop;

static bool post(const cmd_t *cmd) {
    if (!cmd || !s_queue) return false;
    if (xQueueSend(s_queue, cmd, 0) != pdTRUE) {
        // Lost event invalidates the whole session. Never claim SAVED.
        atomic_store(&s_drop, true);
        ESP_LOGE(TAG, "SD queue full: recording marked incomplete");
        return false;
    }
    return true;
}

lasr_sd_status_t lasr_sd_service_status(void) {
    return (lasr_sd_status_t)atomic_load(&s_state);
}

bool lasr_sd_service_begin(const char *target, uint32_t revision, uint32_t delay_ms) {
    if (!target || strlen(target)>=LASR_TARGET_MAX) return false;
    cmd_t cmd={.type=CMD_BEGIN,.revision=revision,.delay_ms=delay_ms};
    (void)snprintf(cmd.target,sizeof(cmd.target),"%s",target);
    return post(&cmd);
}

bool lasr_sd_service_shot(const lasr_shot_t *shot) {
    if (!shot) return false;
    cmd_t cmd={.type=CMD_SHOT,.shot=*shot};
    return post(&cmd);
}

bool lasr_sd_service_finish(uint64_t elapsed) {
    cmd_t cmd={.type=CMD_FINISH,.final_elapsed_ms=elapsed};
    return post(&cmd);
}

bool lasr_sd_service_abort(void) {
    const cmd_t cmd={.type=CMD_ABORT};
    return post(&cmd);
}

bool lasr_sd_service_probe(void) {
    const cmd_t cmd={.type=CMD_TEST};
    return post(&cmd);
}

bool lasr_sd_service_retry(void) {
    const cmd_t cmd={.type=CMD_RETRY};
    return post(&cmd);
}

static bool test_card(void) {
    // The diagnostic writes outside /LASR/sessions and never invents shot data.
    const char *path=BSP_SD_MOUNT_POINT "/LASR/.write-test";
    FILE *fp=fopen(path,"wb");
    if(!fp) return false;
    bool ok=fwrite("LASR SD TEST\n",1,13,fp)==13 && fflush(fp)==0;
    if(ok) ok=fsync(fileno(fp))==0;
    if(fclose(fp)!=0) ok=false;
    if(!ok) {remove(path);return false;}
    fp=fopen(path,"rb");
    if(!fp) return false;
    char data[14]={0};
    ok=fread(data,1,13,fp)==13 && strcmp(data,"LASR SD TEST\n")==0;
    if(fclose(fp)!=0) ok=false;
    if(remove(path)!=0) ok=false;
    return ok;
}

static void worker(void *unused) {
    (void)unused;
    atomic_store(&s_state,LASR_SD_MOUNTING);
    bool mounted=bsp_sdcard_mount()==ESP_OK;
    if (!mounted) ESP_LOGW(TAG,"No readable microSD; retry is available");
    atomic_store(&s_state,mounted?LASR_SD_READY:LASR_SD_NO_CARD);
    lasr_session_t session={0};
    for (;;) {
        cmd_t cmd={0};
        if (xQueueReceive(s_queue,&cmd,portMAX_DELAY)!=pdTRUE) continue;
        if (!mounted && (cmd.type==CMD_RETRY || cmd.type==CMD_TEST || cmd.type==CMD_BEGIN)) {
            atomic_store(&s_state,LASR_SD_MOUNTING);
            mounted=bsp_sdcard_mount()==ESP_OK;
            atomic_store(&s_state,mounted?LASR_SD_READY:LASR_SD_NO_CARD);
        }
        if (!mounted) continue;
        if (atomic_exchange(&s_drop,false)) {
            lasr_store_abort(&session);
            atomic_store(&s_state,LASR_SD_INCOMPLETE);
            ESP_LOGE(TAG,"SD writer event queue overflow; session incomplete");
        }
        if (cmd.type==CMD_BEGIN && session.phase!=LASR_STORE_RECORDING) {
            if (!lasr_store_begin(&session,BSP_SD_MOUNT_POINT,cmd.target,
                                  cmd.revision,cmd.delay_ms)) {
                ESP_LOGE(TAG,"Session start failed: %s",session.last_error);
                atomic_store(&s_state,LASR_SD_ERROR);
            } else atomic_store(&s_state,LASR_SD_RECORDING);
        } else if(cmd.type==CMD_SHOT && session.phase==LASR_STORE_RECORDING) {
            if(!lasr_store_shot(&session,&cmd.shot)) {
                ESP_LOGE(TAG,"Shot write rejected or failed: %s",session.last_error);
                lasr_store_abort(&session);atomic_store(&s_state,LASR_SD_INCOMPLETE);
            }
        } else if(cmd.type==CMD_FINISH && session.phase==LASR_STORE_RECORDING) {
            const bool ok=lasr_store_finish(&session,cmd.final_elapsed_ms);
            if(!ok) ESP_LOGE(TAG,"Session finish failed: %s",session.last_error);
            atomic_store(&s_state,ok?LASR_SD_SAVED:LASR_SD_ERROR);
        } else if(cmd.type==CMD_ABORT) {
            lasr_store_abort(&session);atomic_store(&s_state,LASR_SD_INCOMPLETE);
        } else if(cmd.type==CMD_RETRY && session.phase!=LASR_STORE_RECORDING) {
            atomic_store(&s_state,LASR_SD_READY);
        } else if(cmd.type==CMD_TEST && session.phase!=LASR_STORE_RECORDING) {
            atomic_store(&s_state,test_card()?LASR_SD_TEST_OK:LASR_SD_ERROR);
        }
    }
}

bool lasr_sd_service_init(void) {
    if(s_queue) return true;
    s_queue=xQueueCreate(32,sizeof(cmd_t));
    if(!s_queue){ESP_LOGE(TAG,"Cannot allocate SD command queue");return false;}
    if(xTaskCreate(worker,"lasr_sd",6144,NULL,2,NULL)!=pdPASS) {
        ESP_LOGE(TAG,"Cannot launch SD writer");vQueueDelete(s_queue);s_queue=NULL;return false;
    }
    return true;
}