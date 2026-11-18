#include "lasr_usb_serial.h"
#include "lasr_usb_wire.h"
#include "lasr_sd_service.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static const char *TAG="lasr_usb";

static bool allow_transfer(void) {
    const lasr_sd_status_t s=lasr_sd_service_status();
    return s==LASR_SD_READY || s==LASR_SD_SAVED || s==LASR_SD_TEST_OK;
}

static void list_sessions(void) {
    if(!allow_transfer()){puts("!LASR|ERROR|SD_NOT_READY");fflush(stdout);return;}
    DIR *dir=opendir(BSP_SD_MOUNT_POINT "/LASR/sessions");
    if(!dir){puts("!LASR|ERROR|NO_SESSIONS");fflush(stdout);return;}
    puts("!LASR|LIST_BEGIN");
    struct dirent *entry;
    while((entry=readdir(dir))!=NULL) {
        if(lasr_usb_safe_session(entry->d_name) &&
           lasr_usb_verified(BSP_SD_MOUNT_POINT,entry->d_name))
            printf("!LASR|ITEM|%s\n",entry->d_name);
    }
    if(closedir(dir)!=0)ESP_LOGW(TAG,"Session index close failed");
    puts("!LASR|LIST_END");fflush(stdout);
}

static void process(const char *line) {
    char command[12]={0},id[24]={0},file[24]={0};
    const int parsed=sscanf(line,"LASR1 %11s %23s %23s",command,id,file);
    if(parsed==1&&strcmp(command,"LIST")==0) {list_sessions();return;}
    if(parsed==3&&strcmp(command,"GET")==0) {
        if(!allow_transfer()){puts("!LASR|ERROR|SD_BUSY_OR_MISSING");return;}
        if(!lasr_usb_send_file(stdout,BSP_SD_MOUNT_POINT,id,file))
            puts("!LASR|ERROR|FILE_NOT_VERIFIED_OR_UNAVAILABLE");
        fflush(stdout);return;
    }
    puts("!LASR|ERROR|UNKNOWN_COMMAND");fflush(stdout);
}

static void worker(void *unused) {
    (void)unused;
    // Console is already USB Serial/JTAG (CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG).
    // Only the cable-connected Windows utility sends LASR1 commands.
    setvbuf(stdin,NULL,_IONBF,0);
    char line[100];
    for(;;) {
        if(!fgets(line,sizeof(line),stdin)) {
            clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(50));continue;
        }
        if(strncmp(line,"LASR1 ",6)!=0)continue;
        ESP_LOGI(TAG,"Received local USB session-transfer request");
        process(line);
    }
}

bool lasr_usb_serial_start(void) {
    if(xTaskCreate(worker,"lasr_usb",4096,NULL,2,NULL)!=pdPASS){
        ESP_LOGE(TAG,"USB file-transfer task could not start");return false;
    }
    ESP_LOGI(TAG,"LASR1 cable session export ready (Serial/JTAG, not MSC)");
    return true;
}