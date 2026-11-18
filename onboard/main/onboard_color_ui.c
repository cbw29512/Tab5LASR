#include "onboard_color_ui.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "lasr_color_ui";
/* Photo evidence: vivid green/cyan image is consistent with RGB565 byte-order confusion.
 * Begin with byte correction; the on-screen button can still test all four modes.
 * This is a provisional display setting until verified against another photo. */
static onboard_color_mode_t g_mode = ONBOARD_COLOR_SWAP_BYTES;
static lv_obj_t *g_button_label;

onboard_color_mode_t onboard_color_ui_mode(void) {
    // Called by the frame worker while it owns the LVGL display lock.
    return g_mode;
}

static void update_button(void) {
    if (!g_button_label) return;
    char label[64] = {0};
    const int written = snprintf(label, sizeof(label), "COLOR %u/4: %s",
                                 (unsigned)g_mode + 1,
                                 onboard_color_mode_name(g_mode));
    if (written <= 0 || written >= (int)sizeof(label)) {
        ESP_LOGE(TAG, "Failed to format selected color mode");
        return;
    }
    lv_label_set_text(g_button_label, label);
}

static void next_mode(lv_event_t *event) {
    (void)event;
    g_mode = (onboard_color_mode_t)(((int)g_mode + 1) % ONBOARD_COLOR_MODE_COUNT);
    update_button();
    ESP_LOGI(TAG, "Color diagnostic: mode=%u %s",
             (unsigned)g_mode + 1, onboard_color_mode_name(g_mode));
}

static bool add_reference(lv_obj_t *panel, int32_t x, lv_color_t color,
                          const char *letter) {
    lv_obj_t *patch = lv_obj_create(panel);
    if (!patch) {
        ESP_LOGE(TAG, "Failed to create reference color patch %s", letter);
        return false;
    }
    lv_obj_set_size(patch, 62, 42);
    lv_obj_align(patch, LV_ALIGN_TOP_LEFT, x, 84);
    lv_obj_set_style_bg_color(patch, color, 0);
    lv_obj_set_style_bg_opa(patch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(patch, 0, 0);
    lv_obj_clear_flag(patch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = lv_label_create(patch);
    if (!label) {
        ESP_LOGE(TAG, "Failed to create reference color label %s", letter);
        return false;
    }
    lv_label_set_text(label, letter);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    return true;
}

bool onboard_color_ui_attach(lv_obj_t *screen) {
    // The panel tests the display independently of the incoming camera pixels.
    // Caller holds the LVGL lock; battery controls remain on the same screen.
    if (!screen) {
        ESP_LOGE(TAG, "Camera color UI screen is unavailable");
        return false;
    }
    lv_obj_t *panel = lv_obj_create(screen);
    if (!panel) {
        ESP_LOGE(TAG, "Could not create camera color diagnostic panel");
        return false;
    }
    lv_obj_set_size(panel, 402, 154);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);

    lv_obj_t *button = lv_button_create(panel);
    if (!button) {
        ESP_LOGE(TAG, "Could not create color cycle button");
        return false;
    }
    lv_obj_set_size(button, 354, 65);
    lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_event_cb(button, next_mode, LV_EVENT_CLICKED, NULL);
    g_button_label = lv_label_create(button);
    if (!g_button_label) {
        ESP_LOGE(TAG, "Could not create color mode text label");
        return false;
    }
    lv_obj_center(g_button_label);
    update_button();

    // Reference patches are pure LVGL colors, NOT camera output.
    // A photo of these patches tells us if the LCD itself is correct.
    if (!add_reference(panel, 17, lv_color_hex(0xFF0000), "R") ||
        !add_reference(panel, 101, lv_color_hex(0x00FF00), "G") ||
        !add_reference(panel, 185, lv_color_hex(0x0000FF), "B")) {
        return false;
    }
    ESP_LOGI(TAG, "Reference RGB swatches and four-mode camera test attached");
    return true;
}
