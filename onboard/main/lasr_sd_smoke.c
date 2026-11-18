#include "lasr_sd_smoke.h"
#include "lasr_sd_smoke_io.h"
#include "lasr_sd_device.h"
#include "esp_log.h"
#include "esp_random.h"
#include <stdio.h>

static const char *TAG="lasr_sd_smoke";

esp_err_t lasr_sd_smoke_once(void) {
    if (lasr_sd_is_mounted()) {
        ESP_LOGE(TAG,"SD already owned; refusing diagnostic");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t status=lasr_sd_mount_noformat();
    if (status!=ESP_OK)return status;
    bool passed=false;
    for (unsigned attempt=0;attempt<4 && !passed;++attempt) {
        char id[24]={0};
        const int n=snprintf(id,sizeof(id),"%08lx-%u",
                           (unsigned long)esp_random(),attempt);
        if (n<=0 || n>=(int)sizeof(id))break;
        passed=lasr_sd_smoke_file(lasr_sd_root(),id);
    }
    if (!passed)ESP_LOGE(TAG,"SD diagnostic write/verify failed (no formatting)");
    const esp_err_t unmount=lasr_sd_unmount();
    if (unmount!=ESP_OK) {
        ESP_LOGE(TAG,"SD unmount failed; do not start USB export");
        return unmount;
    }
    if (!passed)return ESP_FAIL;
    ESP_LOGI(TAG,"microSD unique-file write/readback/removal and unmount PASSED");
    return ESP_OK;
}