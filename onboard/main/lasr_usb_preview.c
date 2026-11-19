#include "lasr_usb_preview.h"
#include "lasr_usb_preview_wire.h"
#include "sdkconfig.h"
#if CONFIG_LASR_USB_PREVIEW_DIAGNOSTIC
#include "driver/jpeg_encode.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

#define PREVIEW_W 320u
#define PREVIEW_H 176u
#define INPUT_BYTES (PREVIEW_W*PREVIEW_H*2u)
#define JPG_MAX 65536u
#define WIRE_CAP (JPG_MAX*4u/3u+200u)
static const char *TAG="lasr_usb_video";
static jpeg_encoder_handle_t s_encoder;
static uint16_t *s_input;
static uint8_t *s_jpg;
static char *s_wire;
static size_t s_input_cap,s_jpg_cap;
static volatile int s_frame_state; // 0 idle, 1 copying, 2 ready, 3 sending
static uint32_t s_seq,s_offer_count;
static bool s_active;

static void usb_frame_worker(void *unused) {
    (void)unused;
    const jpeg_encode_cfg_t cfg={
        .src_type=JPEG_ENCODE_IN_FORMAT_RGB565,
        .sub_sample=JPEG_DOWN_SAMPLING_YUV420,
        .image_quality=65,.width=PREVIEW_W,.height=PREVIEW_H,
        .pixel_reverse=false
    };
    for (;;) {
        int expected=2;
        if (!__atomic_compare_exchange_n(&s_frame_state,&expected,3,false,
                                         __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
            vTaskDelay(pdMS_TO_TICKS(15));continue;
        }
        uint32_t jpg_size=0;
        const esp_err_t result=jpeg_encoder_process(s_encoder,&cfg,
            (const uint8_t *)s_input,INPUT_BYTES,s_jpg,s_jpg_cap,&jpg_size);
        if (result!=ESP_OK || jpg_size>JPG_MAX) {
            ESP_LOGW(TAG,"JPEG preview failed: %s (%lu bytes)",
                esp_err_to_name(result),(unsigned long)jpg_size);
        } else {
            const size_t n=lasr_usb_frame_format(s_jpg,jpg_size,++s_seq,
                PREVIEW_W,PREVIEW_H,s_wire,WIRE_CAP);
            if (!n) {
                ESP_LOGW(TAG,"JPEG wire packet rejected: %lu bytes",
                    (unsigned long)jpg_size);
            } else {
                // A single USB serial console is shared with ESP_LOG.
                // Serialize stdout writes; browser independently verifies
                // JPEG length, markers and FNV-1a to reject torn frames.
                flockfile(stdout);
                const size_t sent=fwrite(s_wire,1,n,stdout);
                fflush(stdout);
                funlockfile(stdout);
                if (sent!=n) ESP_LOGW(TAG,"Incomplete USB frame write");
            }
        }
        __atomic_store_n(&s_frame_state,0,__ATOMIC_RELEASE);
    }
}

bool lasr_usb_preview_start(void) {
    jpeg_encode_memory_alloc_cfg_t in={.buffer_direction=JPEG_ENC_ALLOC_INPUT_BUFFER};
    jpeg_encode_memory_alloc_cfg_t out={.buffer_direction=JPEG_ENC_ALLOC_OUTPUT_BUFFER};
    s_input=jpeg_alloc_encoder_mem(INPUT_BYTES,&in,&s_input_cap);
    s_jpg=jpeg_alloc_encoder_mem(JPG_MAX,&out,&s_jpg_cap);
    s_wire=heap_caps_malloc(WIRE_CAP,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!s_input || !s_jpg || !s_wire ||
        s_input_cap<INPUT_BYTES || s_jpg_cap<JPG_MAX) {
        ESP_LOGE(TAG,"Insufficient aligned buffers for USB camera JPEG");
        free(s_input);free(s_jpg);free(s_wire);
        s_input=NULL;s_jpg=NULL;s_wire=NULL;
        return false;
    }
    const jpeg_encode_engine_cfg_t engine_cfg={.intr_priority=0,.timeout_ms=100};
    esp_err_t err=jpeg_new_encoder_engine(&engine_cfg,&s_encoder);
    if (err!=ESP_OK) {
        ESP_LOGE(TAG,"JPEG hardware unavailable: %s",esp_err_to_name(err));
        free(s_input);free(s_jpg);free(s_wire);
        s_input=NULL;s_jpg=NULL;s_wire=NULL;
        return false;
    }
    if (xTaskCreate(usb_frame_worker,"lasr_usb_video",12*1024,NULL,
                    tskIDLE_PRIORITY+1,NULL)!=pdPASS) {
        ESP_LOGE(TAG,"USB camera worker task allocation failed");
        jpeg_del_encoder_engine(s_encoder);s_encoder=NULL;
        free(s_input);free(s_jpg);free(s_wire);
        s_input=NULL;s_jpg=NULL;s_wire=NULL;
        return false;
    }
    s_active=true;
    ESP_LOGW(TAG,"USB CAMERA LIVE PREVIEW: 320x176 JPEG from real CSI LCD pixels");
    return true;
}

void lasr_usb_preview_offer(const uint16_t *src,uint32_t sw,uint32_t sh) {
    if (!s_active || !s_input || !s_encoder || !src || !sw || !sh ||
        (++s_offer_count%3u)!=0) return;
    int expected=0;
    if (!__atomic_compare_exchange_n(&s_frame_state,&expected,1,false,
                                     __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) return;
    // Copy while the LVGL frame is still owned by the capture task.
    // No borrowed V4L2 memory survives a QBUF and the JPEG task sees
    // only an owned, 320x176 downsampled buffer.
    for(uint32_t y=0;y<PREVIEW_H;y++) {
        const uint32_t sy=(uint32_t)((uint64_t)y*sh/PREVIEW_H);
        for(uint32_t x=0;x<PREVIEW_W;x++) {
            const uint32_t sx=(uint32_t)((uint64_t)x*sw/PREVIEW_W);
            s_input[y*PREVIEW_W+x]=src[sy*sw+sx];
        }
    }
    __atomic_store_n(&s_frame_state,2,__ATOMIC_RELEASE);
}
#else
bool lasr_usb_preview_start(void) { return false; }
void lasr_usb_preview_offer(const uint16_t *src,uint32_t w,uint32_t h) {
    (void)src;(void)w;(void)h;
}
#endif
