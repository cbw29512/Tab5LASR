#include "lasr_sd_status_ui.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "lasr_sd_ui";

void lasr_sd_status_ui_show(bool success) {
    if (!bsp_display_lock(5000)) {
        ESP_LOGE(TAG, "Could not lock display for SD test status");
        return;
    }
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *panel = lv_obj_create(screen);
    if (!panel) {
        ESP_LOGE(TAG, "Could not create SD diagnostic screen panel");
        bsp_display_unlock();
        return;
    }
    lv_obj_set_size(panel, 700, 280);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = lv_label_create(panel);
    if (!label) {
        ESP_LOGE(TAG, "Could not create SD diagnostic status label");
        bsp_display_unlock();
        return;
    }
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
#else
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
#endif
    lv_obj_set_width(label, 650);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
    lv_label_set_text(label, success ?
        "CAMERA + microSD: SD PASS\n"
        "Synthetic session persisted; CSI should keep streaming.\n"
        "SIRT candidates are NOT saved as real shots." :
        "CAMERA + microSD: SD FAILED\n"
        "Do not claim combined physical test passed.\n"
        "No formatting or deletion was attempted.");
#else
    lv_label_set_text(label, success ?
        "microSD DIAGNOSTIC: PASS\n"
        "Read/write and synthetic session verified.\n"
        "Camera disabled in this TEST firmware.\n"
        "Synthetic files: /LASR/diagnostics/" :
        "microSD DIAGNOSTIC: FAILED\n"
        "Check card, format, and diagnostic logs.\n"
        "No automatic formatting or deletion.\n"
        "Camera disabled in this TEST firmware.");
#endif
    lv_obj_center(label);
    bsp_display_unlock();
}