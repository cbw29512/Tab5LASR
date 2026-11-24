#include "lasr_sd_device.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG="lasr_sd";
static bool s_mounted;

esp_err_t lasr_sd_mount_noformat(void) {
    if (s_mounted) {
        ESP_LOGW(TAG,"SD already mounted by LASR owner");
        return ESP_ERR_INVALID_STATE;
    }
    sdmmc_host_t host=SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot=SDMMC_SLOT_CONFIG_DEFAULT();
    // Use the pinned Tab5 BSP's actual board pin configuration.
    bsp_sdcard_get_sdmmc_host(0,&host);
    bsp_sdcard_sdmmc_get_slot(0,&slot);
    const esp_vfs_fat_sdmmc_mount_config_t mount={
        .format_if_mount_failed=false, // Never destroy an unrecognized user card.
        .max_files=6,
        .allocation_unit_size=0
    };
    bsp_sdcard_cfg_t config={.mount=&mount,.host=&host,.slot.sdmmc=&slot};
    const esp_err_t err=bsp_sdcard_sdmmc_mount(&config);
    if (err!=ESP_OK) {
        ESP_LOGE(TAG,"microSD mount failed: %s; card is NOT formatted",esp_err_to_name(err));
        return err;
    }
    s_mounted=true;
    ESP_LOGI(TAG,"microSD mounted at %s",BSP_SD_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t lasr_sd_unmount(void) {
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    const esp_err_t err=bsp_sdcard_unmount();
    if (err!=ESP_OK) {
        ESP_LOGE(TAG,"microSD unmount failed: %s",esp_err_to_name(err));
        return err;
    }
    s_mounted=false;
    ESP_LOGI(TAG,"microSD safely unmounted");
    return ESP_OK;
}

bool lasr_sd_is_mounted(void) {return s_mounted;}
const char *lasr_sd_root(void) {return BSP_SD_MOUNT_POINT;}