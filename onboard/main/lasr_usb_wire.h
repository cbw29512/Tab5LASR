#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Read-only USB serial protocol; no arbitrary paths or deletes.
bool lasr_usb_safe_session(const char *session_id);
bool lasr_usb_safe_file(const char *filename);
bool lasr_usb_verified(const char *root, const char *session_id);
bool lasr_usb_send_file(FILE *out, const char *root,
                        const char *session_id, const char *filename);
uint32_t lasr_usb_crc32(uint32_t state, const void *data, size_t bytes);