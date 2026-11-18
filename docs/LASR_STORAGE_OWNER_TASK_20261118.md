# LASR storage owner task — asynchronous recorder integration

Date: 2026-11-18. Review branch: `feat/lasr-microsd-session-core-20261118`.

## Definition of Done (this software slice)

1. A **single SD owner task**, when explicitly initialized by a future hardware-ready UI integration, receives session BEGIN / SHOT / FINISH messages through a bounded (32-message) FreeRTOS queue.
2. Calls from the camera/detector path use zero-wait queue submission. No microSD `fprintf`, `fflush`, `fsync`, mount/unmount or file close occurs in the camera capture loop.
3. The worker mounts microSD only on explicit BEGIN, using the previously verified `lasr_sd_mount_noformat()` adapter, then writes real shot events with the existing verified journal writer.
4. FIFO and in-flight sender accounting prevent a FINISH from overtaking an accepted shot. Any queue-overflow or missed shot permanently prevents `SAVED`, retaining the pending journal and returning `UNSAVED`.
5. Firmware build and existing 1 MiB app partition safety gates continue to pass; camera preview, charging, Color 2/4 and power-recovery code are untouched.
6. Software CI tests validate event schema and process transitions (valid session, unsaved when overflow, no overwrite, invalid sequence). Firmware CI validates linking. **Passing these tests is not a physical microSD acceptance test.**

## Data schema and state

`lasr_rec_message_t { type: BEGIN|SHOT|FINISH, payload: { begin: { session_id, target_id, calibration_revision, delay_ms } | shot: lasr_shot_t } }`.

`lasr_recorder_core_t { state, store, discarded_events }`.

States: `IDLE -> STARTING -> RECORDING -> FINALIZING -> SAVED` or `UNSAVED`. `UNSAVED` is irreversible for that session; a new session uses a different unique identifier.

Public C interface `lasr_recorder_task_init/begin/shot/finish/status/dropped`. It is intentionally **not called from `app_main()`**, so the currently working Tab5 image remains unchanged until hardware setup, touchscreen controls, raw-SIRT pulse detection and system tests are ready.

## Pseudocode

```text
UI asks BEGIN(session,target,rev,delay)
    -> enqueue BEGIN (reject if writer unavailable or invalid state)
SD worker consumes BEGIN
    -> mount card with format_if_mount_failed = false
    -> create unique /LASR/sessions/<id>/shots.pending
Camera event source observes a validated laser transition
    -> timestamp/split is computed by shot timer before queue submission
    -> try enqueue SHOT without blocking camera
    -> on queue full, increment dropped counter
SD worker receives each SHOT
    -> append to journal; validate order, timing and normalized coordinates
UI asks FINISH
    -> close intake, wait for in-flight sends, enqueue FINISH
SD worker on FINISH
    -> if any lost shot: abort / retain pending / UNSAVED
    -> otherwise flush, sync, verify CSV/JSON and only then SAVED
```

## Files

- `onboard/main/lasr_recorder_core.{h,c}`: portable, host-testable command-to-session state machine.
- `onboard/main/lasr_recorder_task.{h,c}`: bounded FreeRTOS queue, dedicated SD task, nonblocking shot submission, race-aware FINISH gate.
- `onboard/tests/test_lasr_recorder_core.c`: host tests for success, journal retention on loss, malformed event ordering, and existing-ID rejection.
- `.github/workflows/lasr-session-storage-tests.yml`: extended strict GCC/UBSan suite.
- `onboard/main/CMakeLists.txt`: builds modules into ESP32-P4 firmware, but never calls task init during boot.

## Explicit remaining blockers

- **Not receiving physical laser pulses.** No camera SIRT detector has yet been certified. The caller must supply source-accurate events; do not manufacture timed shots or scores.
- No touchscreen 'Record' wiring and no device-side storage status UI yet. Explicit init is deliberately disabled.
- Queue saturation, SD latency, storage-full, card removal, eject and power interruption must be tested physically. Existing journal recovery is best effort and requires a recovery parser/UX.
- No USB MSC device driver, safe USB eject callbacks, Wi-Fi session downloader, or microSD video recording here. USB ownership policy remains separate.
- `xQueueSend` API is called from **task context only**; for ISR integration use the separate verified `FromISR` API, not this function.
- SD card must be FAT readable and must never be auto-formatted. Session logs and file transfer remain local; no cloud dependency.
- PR #6 camera Wi-Fi streaming remains blocked by its own image/ESP32-C6 compatibility. No application-only flash or partition change is authorized from this PR.

## Next physical milestones

1. Add a dry-run on-device microSD mount + read/write/readback diagnostic behind a deliberate hardware test command. Preserve existing data, never auto-format.
2. Implement real laser rising-edge detector using the onboard raw CSI frames, with anti-duplicate rearm and measured timing; wire to `lasr_recorder_task_shot`.
3. Add touchscreen Start/Stop/storage badge; test card-missing, card-full and battery unplug with hardware.
4. Implement independent session readback, then USB-C / Wi-Fi copy using the documented ownership contracts.

References: `docs/LASR_MICROSD_SESSION_STORAGE_CONTRACT.md` and `docs/LASR_SESSION_TRANSFER_CONTRACT.md` on browser MVP PR #7; Espressif ESP32-P4 FreeRTOS and Tab5 BSP 1.3.3 API.