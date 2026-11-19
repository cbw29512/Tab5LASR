# TAB5 LASR: physical USB camera picture and SIRT candidate overlay

## Objective / definition of done
The shooter must see **actual pixels from the built-in Tab5 CSI camera** on
the Windows browser and be able to correlate *uncalibrated* SIRT candidate
coordinates with the pictured target. The existing serial-only browser
cannot fulfill this because it receives only text telemetry.

This is a dedicated, review-only USB camera test; it is NOT the finished
Wi-Fi viewer, target calibration, scored sessions or microSD live recording.
Do not assume it works on hardware until physically tested.

## Data schema and state
- Source: the existing live LCD RGB565 view **after actual camera PPA rotation
  and the operator's confirmed Color 2 correction**; no browser camera.
- RGB input snapshot: copied into a **320x176 owned buffer**, at most every
  third completed displayed frame. The V4L2 camera buffer is never retained.
- JPEG: ESP32-P4 **hardware JPEG encoder**, quality 65, YUV420, 64 KiB
  bounded output buffer, run in a separate FreeRTOS worker.
- Wire: one ASCII line `LASRJPEGV1 seq width height jpeg_bytes fnv32 base64\n`
  over already-proven USB Serial/JTAG. Browser verifies length, both JPEG
  delimiters and FNV-1a before it shows the frame. Corrupted/interleaved
  or stale frames must not display as fresh camera data.
- SIRT candidates: existing **UNVERIFIED** timestamp/x_norm/y_norm logs;
  visual overlay is only a raw-coordinate guide. Rotation must be checked
  on physical paper, not assumed calibrated.
- State: camera STREAMING -> frame copy IDLE/FILLING/READY/SENDING -> JPEG
  encoded -> USB frame received -> checksum checked -> JPEG decoded -> VIEW.
  On failure, report missing frames and preserve local camera and battery UI.
- Battery recovery: preserve existing guarded charging and manual recovery.
- Storage: the USB-preview engineering profile deliberately omits the
  combined synthetic SD smoke to fit within the original 1 MiB app partition.

## Safety and CI gates
1. Firmware is built ONLY under explicit opt-in
   `CONFIG_LASR_USB_PREVIEW_DIAGNOSTIC=y` with raw laser scan enabled.
2. CMake dependency is the official ESP-IDF 6.1 `esp_driver_jpeg`.
3. All builds require native ESP32-P4 v1.x image, strict <=1048576-byte
   factory app partition, and unmodified bootloader/partition table.
4. `PREFLIGHT_LASR_DIAGNOSTIC.ps1` must compare the connected **complete
   16 MiB flash** with the verified original camera recovery plus the exact
   installed combined diagnostic, before allowing an app-only write.
5. Candidate artifact has mode `usb-preview`, source SHA, SHA256SUMS,
   `FLASH=NO_AUTO_FLASH`, and manual typed `FLASH` confirmation.
6. On hardware verify power, real camera rendering, FPS, JPEG updates,
   SIRT candidate coordinates/overlay, disconnect/reconnect and USB
   throughput. Do NOT count GitHub green checks as physical success.

## Browser
Use the existing local Windows launcher `tools/OPEN_LASR_USB_TEST.ps1` on
a fresh port while another launcher remains active. The new
`web/serial-test.html` shows the actual camera JPEG above the raw hit grid.
There is no actual Tab5 Wi-Fi server in this firmware. Future portable
operation should work without a PC, but first prove physical USB preview.
