# Tab5 LASR — onboard camera capability baseline (priority M1)

The first physical camera test uses the Tab5 **built-in MIPI-CSI camera**. No USB camera is needed. The older USB-UVC prototype remains in `../firmware/` as a fallback; **do not flash that build for the onboard test**.

## Definition of Done (M1 onboard camera)

1. Boot on ESP32-P4 rev v1.3 after application-only flash; installed dice roller is preserved in the verified **private full-device backup**.
2. Keep the factory **P4 idle LOW** power-latch and the normal-charge IP2326 P7/P5 safeguards. Report INA226 bus and shunt readings; no unattended battery-recovery attempt.
3. Initialize the exact onboard sensor through `bsp_camera_start()` and Espressif `esp_video` / MIPI-CSI. Detect pixel format and frame size from V4L2, not an assumed USB MJPEG format.
4. Show live preview on the Tab5 LCD, measure actual received/rendered FPS independently over five-second windows, and log frame errors.
5. Run on USB-C first, then verify **battery-only camera preview** and cold boot physically. A passing CI build alone is not proof of battery capability.
6. Camera model mismatch is possible: marketing docs refer to SC2356 but Espressif 1.3.3 BSP config uses `CONFIG_CAMERA_SC202CS`. Record sensor boot logs; never silently substitute a driver.
7. After this works, implement LAN Wi-Fi browser preview using captured sensor frames, then laser detection.

## Data schema and state — prior to implementation

`onboard_camera_status_t { state, negotiated_width, negotiated_height, v4l2_pixelformat, captured_frames, displayed_frames, failed_frames, last_frame_monotonic_us }`.

`state = BOOT | CAMERA_INIT | FORMAT_READY | STREAMING | ERROR`.

Owned framebuffer lifecycle: driver-allocated mmap V4L2 frame → optional PPA convert/rotation into PSRAM LVGL canvas buffer → requeue driver frame. The preview stores a separate buffer (never a dangling pointer into a requeued sensor buffer). The LCD task and producer synchronize through the BSP LVGL mutex.

## Logic flow (pseudocode)

- Display/BSP I²C initialize; power-latch P4 LOW; safe charger startup; battery monitor.
- Initialize CSI sensor via BSP and chosen Espressif sensor driver.
- Query supported V4L2 capture format, frame dimensions; select color conversion (RGB565, RGB565X, or UYVY).
- Create camera canvas and PPA client, request at least two capture buffers and queue them.
- Stream: DQBUF → rotate/fit/convert via PPA → update canvas → QBUF; log FPS each 5 seconds.
- On errors log sensor/driver details, stop capture, don't invent frames; never change charging pins to repair a camera problem.

## Hardware gates

- Exact permanent flash backup is archived privately as `cbw29512/Tab5DiceRecovery` release `tab5-dice-readback-20261117-173241`; user confirmed SHA-256 `52545123E6C20C4B139F290B64E4C70701C3618616D9A4149D444039CE7A3C01`.
- The previous camera flasher printed `Cancelled; no flash was written` after a blank confirmation prompt. Dice roller remains installed.
- All firmware modifications must remain **application-only** at offset `0x10000`, and the image must fit the independently verified 1 MiB factory partition.
- Do not assume the on-device sensor matches the BSP until proven by logs.
