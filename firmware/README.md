# Camera-only capability test (firmware M1)

This firmware **replaces** the dice-roller application. It does not install a dual-boot menu, keep the dice controls, run scoring, or stream video over Wi-Fi yet. The existing dice-roller source remains in the original repository.

## Before any flash

Run `tools/BACKUP_TAB5_DICE.ps1` with a private backup repository, and confirm two physical 16 MiB reads have the same SHA-256 and the GitHub download has been reverified. See `docs/FLASH_SAFETY.md`. No installed firmware image has been backed up from this chat.

## Implementation status

- USB Host/UVC stream: **source committed, not yet physically tested**
- UVC matching: any VID:PID, format MJPEG at 320×240 and requested 15 FPS (camera must advertise it)
- Camera pipeline: copies MJPEG frame into PSRAM and immediately returns UVC driver buffer; bounded three-frame queue
- Preview: JPEG decoder → RGB565 Tab5 panel centered at native resolution
- Performance: actual camera callbacks, displayed FPS, dropped frames, decode failures logged every five seconds
- Power: reuses existing working dice-roller factory P4 low + verified initial safe charging controls
- **Known gap:** guarded manual battery-recovery screen is not yet ported; only use this test with a healthy pack and USB available, and re-test power independently
- Web viewer and Wi-Fi: **not yet implemented**

## Firmware architecture

```
M5Stack Tab5 USB-A host
  → UVC MJPEG callback (any camera supporting the selected format)
  → copy to owned PSRAM, return UVC frame immediately
  → bounded FreeRTOS queue
  → frame worker → JPEG decoder → Tab5 panel
                  → real FPS/drop/decode logs over USB Serial/JTAG
```

- Schema in `main/camera_state.h` and `main/camera_metrics.h`
- No camera hardware or Wi-Fi is emulated or claimed as tested
- Each source file stays small; new features should use separate modules after 150 lines
- Errors log `ESP_LOGE` or `ESP_LOGW` and fail safely instead of assuming initialization worked

## Build

GitHub Action: **Tab5 Camera-Only Firmware** builds with ESP-IDF release-v6.1, checks ESP32-P4 revision v1.x compatibility, and enforces the existing 1 MiB factory app size ceiling. A successful run will produce a disposable review artifact with `tab5lasr_camera.bin`. A completed GitHub Actions build is **not proof** of compatibility with a particular physical webcam or power pack.

**Do not flash an unverified or oversized image.** The review build is intentionally *application-only* at the existing verified 0x10000 partition offset, never bootloader/partition table.

## M1 physical acceptance

1. Confirm private permanent dice-roller device backup and app compatibility checks.
2. Plug a UVC USB webcam into the Tab5 USB-A host port.
3. With a safe verified firmware image, flash the *application only*.
4. Observe actual camera preview and serial logs for connected/disconnected behavior.
5. Record `USB camera model, VID:PID, negotiated mode, capture FPS, display FPS, drops, decode failures`.
6. Test camera unplug/replug and USB-to-battery handoff separately.

## M2

Next: local Wi-Fi camera viewer via same-origin `GET /`, `GET /api/camera/status` and `GET /api/camera/stream`; use the same real UVC frames. Not yet coded.
