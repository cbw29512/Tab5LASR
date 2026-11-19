#include "lasr_sd_status_ui.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
#include "lasr_csi_probe.h"
#endif

static const char *TAG = "lasr_sd_ui";
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
static lv_obj_t *s_scan_label;

// GUI callback only toggles an atomic flag. It never scans camera frames or
// writes microSD from the LVGL thread.
static void toggle_scan(lv_event_t *event) {
    (void)event;
    const bool next = !lasr_csi_probe_scanning_enabled();
    lasr_csi_probe_set_scanning_enabled(next);
    if (s_scan_label)
        lv_label_set_text(s_scan_label, next ? "LASER SCAN ON" : "LASER SCAN OFF");
}

static void show_combined(bool success) {
    // Do not create a default white LVGL panel over the camera at all.
    // The video canvas remains underneath a single transparent text label.
    lv_obj_t *status = lv_label_create(lv_scr_act());
    if (!status) { ESP_LOGE(TAG, "Cannot create CSI/SD status label"); return; }
    lv_label_set_text(status, success ? "CSI + SD PASS" : "CSI / SD FAILED");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status,
        success ? lv_color_hex(0x00CC66) : lv_color_hex(0xFF5555), 0);
    lv_obj_set_style_bg_opa(status, LV_OPA_TRANSP, 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_RIGHT, -14, -14);

    // Deliberately small test control: compare camera FPS while raw CSI
    // scanning is OFF versus ON, without another firmware flash.
    lv_obj_t *button = lv_button_create(lv_scr_act());
    if (!button) { ESP_LOGE(TAG, "Cannot create CSI scan toggle"); return; }
    lv_obj_set_size(button, 190, 42);
    lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 14, -8);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x182C35), 0);
    lv_obj_add_event_cb(button, toggle_scan, LV_EVENT_CLICKED, NULL);
    s_scan_label = lv_label_create(button);
    if (!s_scan_label) { ESP_LOGE(TAG, "Cannot create CSI scan toggle label"); return; }
    lv_label_set_text(s_scan_label,
        lasr_csi_probe_scanning_enabled() ? "LASER SCAN ON" : "LASER SCAN OFF");
    lv_obj_center(s_scan_label);
}
#endif

void lasr_sd_status_ui_show(bool success) {
    if (!bsp_display_lock(5000)) {
        ESP_LOGE(TAG, "Cannot lock display for SD test status");
        return;
    }
#if CONFIG_LASR_COMBINED_DIAGNOSTIC
    show_combined(success);
#else
    // The SD-only diagnostic has no camera; its existing full status
    // panel is intentionally retained for that separate test build.
    lv_obj_t *panel = lv_obj_create(lv_scr_act());
    if (!panel) {
        ESP_LOGE(TAG, "Could not create SD-only diagnostic panel");
        bsp_display_unlock();
        return;
    }
    lv_obj_set_size(panel, 700, 280);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = lv_label_create(panel);
    if (!label) {
        ESP_LOGE(TAG, "Could not create SD-only diagnostic label");
        bsp_display_unlock();
        return;
    }
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_width(label, 650);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, success ?
        "microSD DIAGNOSTIC: PASS\n"
        "Read/write and synthetic session verified.\n"
        "Camera disabled in this TEST firmware.\n"
        "Synthetic files: /LASR/diagnostics/" :
        "microSD DIAGNOSTIC: FAILED\n"
        "Check card, format, and diagnostic logs.\n"
        "No automatic formatting or deletion.\n"
        "Camera disabled in this TEST firmware.");
    lv_obj_center(label);
#endif
    bsp_display_unlock();
}
