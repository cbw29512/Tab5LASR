#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t g_camera_frame_queue;
void camera_capture_task(void *ctx);
void camera_usb_library_task(void *ctx);
