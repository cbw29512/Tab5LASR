#include "lasr_sd_widget.h"
#include "lasr_sd_service.h"
#include "esp_log.h"

static const char *TAG="lasr_sd_ui";
static lv_obj_t *s_status;

static const char *label(lasr_sd_status_t state) {
    switch(state) {
    case LASR_SD_MOUNTING: return "SD: CHECKING";
    case LASR_SD_NO_CARD: return "SD: NO CARD / RETRY";
    case LASR_SD_READY: return "SD: READY";
    case LASR_SD_RECORDING: return "SD: RECORDING";
    case LASR_SD_SAVED: return "SD: VERIFIED SAVED";
    case LASR_SD_INCOMPLETE: return "SD: UNSAVED / INCOMPLETE";
    case LASR_SD_ERROR: return "SD: ERROR / UNSAVED";
    case LASR_SD_TEST_OK: return "SD: WRITE TEST PASSED";
    default: return "SD: INITIALIZING";
    }
}

static void refresh(lv_timer_t *timer) {
    (void)timer;
    if (s_status) lv_label_set_text(s_status,label(lasr_sd_service_status()));
}

static void click(lv_event_t *event) {
    (void)event;
    const lasr_sd_status_t status=lasr_sd_service_status();
    bool ok=(status==LASR_SD_NO_CARD)?lasr_sd_service_retry():lasr_sd_service_probe();
    if(!ok) ESP_LOGE(TAG,"SD manual self-test could not be queued");
}

bool lasr_sd_widget_attach(lv_obj_t *screen) {
    if(!screen) { ESP_LOGE(TAG,"SD status requires a display");return false; }
    lv_obj_t *panel=lv_obj_create(screen);
    if(!panel) return false;
    lv_obj_set_size(panel,252,116);
    lv_obj_align(panel,LV_ALIGN_BOTTOM_RIGHT,-12,-12);
    lv_obj_clear_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel,lv_color_hex(0x112830),0);
    lv_obj_set_style_bg_opa(panel,LV_OPA_COVER,0);
    s_status=lv_label_create(panel);
    if(!s_status) return false;
    lv_obj_align(s_status,LV_ALIGN_TOP_MID,0,0);
    lv_label_set_text(s_status,"SD: INITIALIZING");
    lv_obj_t *button=lv_button_create(panel);
    if(!button) return false;
    lv_obj_set_size(button,208,50);
    lv_obj_align(button,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_add_event_cb(button,click,LV_EVENT_CLICKED,NULL);
    lv_obj_t *text=lv_label_create(button);
    if(!text) return false;
    lv_label_set_text(text,"TEST SD / RETRY");
    lv_obj_center(text);
    if(!lv_timer_create(refresh,300,NULL))return false;
    ESP_LOGI(TAG,"Manual microSD probe UI attached; no fake shooter events");
    return true;
}