#pragma once
#include <stdbool.h>
#include <stddef.h>
#define ONBOARD_BUF_COUNT 2

typedef struct { void *data; size_t size; } capture_buf_t;

// Driver-mmap buffers belong to V4L2, not the LVGL preview.
bool onboard_init_buffers(int fd, capture_buf_t *buffers);
void onboard_release_buffers(capture_buf_t *buffers);
