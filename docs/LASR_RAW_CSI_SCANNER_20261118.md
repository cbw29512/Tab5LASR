# Tab5 LASR: opt-in raw-CSI laser candidate scanner (2026-11-18)

## Objective and Definition of Done — software slice

Implement a **source-correct, testable** scanner for the camera formats actually negotiated by `onboard_capture.c` (RGB565, RGB565X and UYVY) without using color-corrected LCD pixels. Convert a verified dark-reference image plus new raw frame into a candidate bright-spot coordinate; pass that to the shared pulse state machine. The normal camera firmware must be unchanged when `CONFIG_LASR_CSI_DIAGNOSTIC=n` (default), and no uncalibrated candidate may be saved to the shooting log.

**This is not a certified real SIRT detector or a finished shooting simulator.** It requires target calibration, physical sensitivity/false-positive testing, a shot clock, accurate target projection and an enabled on-device record UI before scores can be trusted. No device flash is performed by this PR.

## Data schema — before functions

- `lasr_raw_frame_t`: borrowed data pointer, accessible length, stride bytes, width, height, negotiated raw pixel format (RGB565 little-endian, RGB565X big-endian, UYVY).
- `lasr_blob_config_t`: normalized raw-image ROI, minimum luminance, minimum brightness change from dark reference, sampling step, minimum near-peak candidate count, local cluster radius, maximum global candidates. **All thresholds are engineering placeholders**, not tuned for a real camera/laser combination.
- `lasr_blob_result_t`: visible flag, raw-image normalized centroid, global candidate count, cluster count, peak luminance change.
- `lasr_laser_pipeline_t`: generic blob config + source-neutral `lasr_pulse_detector_t` (dark-first, on-frame debounce, held suppression, off-frame rearm) + counters.
- Diagnostic input format is selected at runtime from `V4L2_PIX_FMT_RGB565`, `RGB565X`, or `UYVY`; all other formats fail closed.

## Logic flow (pseudocode)

```text
Device boot -> ESP-IDF sdkconfig LASR_CSI_DIAGNOSTIC defaults false
CSI source negotiates format, width, height, bytesperline
  -> if diagnostic explicitly built ON:
      allocate owned PSRAM baseline buffer, log noncalibrated state
      BEFORE QBUF: receive borrowed V4L2 buffer and monotonic frame time
      first frame: clone into owned baseline (unverified dark reference)
      later frames: validate geometry, stride, bytesused and raw format
       -> compare current luminance to reference within ROI
       -> reject broad lighting changes or dispersed bright points
       -> locate compact bright blob centroid in RAW coordinates
       -> feed source-neutral pulse state machine
       -> log unverified candidate position/time ONLY
      QBUF driver buffer; never hold pointer to driver-owned frame
  -> camera preview, P4/P5/P7 power and color diagnostic unchanged
```

## Implementation

- `lasr_raw_pixel.{h,c}`: bounds-checked RGB565 LE, RGB565X BE and UYVY luma extraction; never assumes the display's Color 2/4 byte swap matches sensor data.
- `lasr_laser_blob.{h,c}`: bounded ROI scanner; compares with external reference; finds a compact changed-bright cluster and rejects area-wide changes.
- `lasr_laser_pipeline.{h,c}`: composes existing source-neutral pulse detector with scanner; **no new duplicate pulse resolver**.
- `lasr_csi_format.{h,c}`: maps negotiated Linux V4L2 fourcc to raw representation.
- `lasr_csi_probe.{h,c}`: opt-in, read-only hardware diagnostic; owns independent PSRAM reference; **does not score, assign shot numbers, or write files**.
- `onboard_capture.c`: guarded hook reads only driver-owned bytes while V4L2 capture buffer remains dequeued, never after QBUF; code disabled by default via `onboard/main/Kconfig.projbuild`.
- Strict synthetic C tests: format/endianness, Y extraction, padded stride, inaccessible buffers, ROI, static bright lights, held laser, rearming, broad lighting shifts.

## Known deficiencies / safety gates

1. A baseline copied from the *first captured frame* is **not a user-confirmed laser-off/dark reference**. This is adequate only for engineering diagnostics. Add explicit dark calibration on the touchscreen before recording real shots.
2. Brightness settings `min_luma=180`, `min_delta=90`, local radius 4, max candidates 120, two ON frames are **placeholder test values**. False positives, ambient changes, sunlight, laser color and camera autoexposure must be evaluated on the user's actual Tab5 and SIRT.
3. A short laser flash can occur between camera frames. No shot reliability/fps claim is made without measured physical data. Compare real pulse duration with actual capture cadence.
4. Diagnostic scanner currently traverses pixels synchronously in the capture loop **when explicitly enabled**; benchmark FPS, latency and power before deployment. Default firmware never executes it.
5. The returned (x,y) is RAW camera normalized coordinates. The existing display PPA rotates/scales the image; correct target homography, rotation and independent persisted alignment must be built before scoring.
6. `shots.pending`, `shots.csv`, session writer and `lasr_recorder_task` are still not started by the running firmware. The camera scanner never submits to them without accurate shot clock/calibration.
7. Continue strict **1 MiB app size gate** and exact ESP32-P4 revision check. Never force partition changes, flash C6, auto-format microSD, or overwrite private full flash backups.
8. **No hardware flashing or merge** from this change. On-device acceptance must validate display orientation/Color 2, battery, startup, first-frame reference, SD mount and real SIRT pulse detection separately.

## Tests and references

Run the host test from `.github/workflows/lasr-session-storage-tests.yml` with GCC `-Wall -Wextra -Werror -fsanitize=undefined`. The PR-triggered ESP-IDF firmware workflow performs an actual ESP32-P4 compilation and refuses an image larger than 1 MiB.

Video format references:
- [Espressif esp_video component formats](https://components.espressif.com/components/espressif/esp_video/versions/2.0.0/changelog?language=en): maps `RGB565` to little endian, `RGB565X` to big endian, and provides UYVY.
- [Linux V4L2 RGB pixel formats](https://cdn.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-rgb.html).
- [Existing onboard CSI camera source](../onboard/main/onboard_capture.c) and [pulse detector](../onboard/main/lasr_laser_pulse.c).

## Device monotonic shot clock (also added in this review)

- `lasr_shot_clock.{h,c}` provides `IDLE -> COUNTDOWN -> ACTIVE -> PAUSED -> COUNTDOWN -> ACTIVE -> COMPLETE` states. It accepts only 3-, 5- or 10-second delays as agreed for the MVP.
- **GO time is supplied by the actual audible-signal caller** when that mechanism is implemented. An elapsed timer must never begin when the Start button is clicked or before the audible GO signal.
- A debounced laser event supplies `first_seen_us`. The clock verifies this occurred on/after actual GO, calculates monotonic elapsed and per-shot split excluding paused/resume-countdown time, and emits a `lasr_shot_t` record with `score=0` and `zone=UNSCORED` until proper camera-to-target scoring is certified.
- No shot is written to microSD by the clock alone. The physical buzzer, on-device Start/Pause/Stop buttons, calibrated homography and writer integration still need separate implementation and hardware tests.
- `test_lasr_shot_clock.c` checks countdown gating, GO origin, duplicate/nonmonotonic events, pause/resume, splits and stop. **Synthetic tests are not physical hardware validation.**

## Explicit microSD physical-test entry point (disabled by default)

- Added `CONFIG_LASR_SD_SMOKE_ON_BOOT` (default `n`) and `lasr_sd_smoke_once()` for a **deliberately enabled** engineering firmware build. After normal display and P4/P5/P7 battery/recovery startup, it mounts microSD using the existing no-format adapter, calls an exclusive-file read/write/verification smoke test under `/LASR/diagnostics`, then unmounts.
- `lasr_sd_smoke_io.{h,c}` is portable and has host tests proving that it rejects path traversal, verifies file content, removes only its own successfully verified test file, and does **not overwrite an existing file**. A failed diagnostic leaves partial test data for investigation rather than declaring success.
- Opting into this mode requires a separate verified device-image build and size/recovery checks; no release, automatic device flash or active-state change is made by adding the code. A passing host test is not proof of physical SD compatibility.
- **No actual shooting sessions are recorded yet.** Existing `lasr_recorder_task` still requires a touchscreen-driven Start/Finish session and calibrated actual laser events.

## Independent opt-in ESP32-P4 build validation

- New `.github/workflows/lasr-hardware-diagnostic-build.yml` compiles **two separate engineering variants**: `sd-smoke` and `raw-csi`. It enables **only one** diagnostic flag per build, so a card readback test does not simultaneously impose raw-camera scanning load.
- Each variant must pass the ESP32-P4 v1.3 compatibility image-info checks and **strict 1 MiB installed application partition size gate**. It publishes only a time-limited review artifact with a SHA-256 manifest and explicit NO_AUTO_FLASH notice; no bootloader, partitions, NVS, C6 Wi-Fi or physical Tab5 are modified.
- Builds prove compile/link/configuration compatibility only, not physical safety, SD card readback, SIRT detection, battery runtime, USB transfer or scores. A verified recovery image, a device-specific flashing procedure and physical acceptance are separately required before using either binary.