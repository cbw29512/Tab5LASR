# Tab5 LASR — microSD firmware implementation record (2026-11-18)

## Objective and Definition of Done
The physical Tab5 stores completed shot sessions on microSD independently of laptop, network or cloud, preserving ordered elapsed/split times, target metadata, hit coordinates and score. The user sees an honest storage state. An interrupted write is recoverable, and copied files remain on the card.

## Source implemented in PR #8
- `lasr_session_store.h`: exact v1 shot/session schema and state enumeration.
- `lasr_session_io.c`, `lasr_session_metadata.c`, `lasr_session_store.c`, `lasr_session_finish.c`: portable filesystem writer and verification.
- `lasr_sd_service.c`: dedicated FreeRTOS SD task and bounded non-blocking command queue. The capture path is not allowed to wait on SD card writes. A dropped event marks the session incomplete.
- `lasr_sd_widget.c`: touchscreen SD state plus explicit **TEST SD / RETRY**. Diagnostic test is **not** labeled as a real shot.
- `main.c`: creates task/widget only after the existing power startup guards, prior to the blocking CSI camera loop.
- `sdkconfig.defaults`: auto-format forbidden; FAT long file names explicitly enabled. Existing camera orientation, COLOR 2/4, battery/charger recovery logic preserved.
- `tools/test_lasr_store_host.sh`: host tests with GCC sanitizers, including invalid and duplicate shot events, completed readback, unique sessions and pending recovery markers.

## Intended SD data (when real shot producer is connected)
```text
/sdcard/LASR/sessions/session-000001/
   target.json           # template ID and calibration revision
   shots.csv             # sequence, elapsed, split, coordinates, zone, score
   session.json           # finalized, readback-verified metadata
```
During recording, `session.pending` and `shots.pending.csv` preserve recovery evidence. No camera video recorded in v1.

## Important limitations
- Physical onboard SIRT laser event detection and touchscreen shooting-session control are **not yet connected** to the recorder queue. This code alone does not create real shooting results.
- `bsp_sdcard_mount()` and files have not yet been exercised on the user's physical microSD card; all filesystem tests have been against an isolated host directory.
- Native Wi-Fi transfer is blocked by separate C6/oversized MJPEG web-firmware issues in PR #6. USB cable transfer is staged separately in stacked PR #9 as a read-only Serial/JTAG protocol, not Mass Storage.
- Neither this branch nor PR #9 has changed or flashed any physical hardware.

## Build evidence and hard stop
- Original enabled-SD ESP-IDF 6.1 build compiled and linked (GitHub run 44272819936), but ESP-IDF's **real factory partition gate rejected it**:
  - Generated application `0x103480` = **1,062,016 bytes**.
  - Factory application partition `0x100000` = **1,048,576 bytes**.
  - Overflow `0x3480` = **13,440 bytes**.
- Size-optimization experiment: `CONFIG_COMPILER_LTO_LINKTIME=y` and `CONFIG_COMPILER_LTO_COMPILETIME=y` added on PR #8; CI results are a separate acceptance gate. Do not assume optimization succeeded.
- If size still exceeds the factory 1MiB partition, **do not flash**; analyze linked component sizes and optimize further or separately review a migration after recovery-layout inspection.
- Preserve verified existing 16MiB backups, P4/P5/P7 safeguards and Color 2/4 preview.

## Hardware acceptance
1. Confirm green compiler, chip revision gate and image size <= 1 MiB.
2. Verify permanent recovery image, app-only update offset and exact device partition map before any flash.
3. On bench: microSD insert/withdraw, write/read test, camera+card, unplugged battery operation, card full/error, repeated reboot and interruption recovery.
4. Implement source-accurate shot detection and session Start/Stop on the Tab5; verify observed laser timing/accuracy vs independent timer.
5. Only then connect USB/ Wi-Fi transfer to verified completed sessions.

Reference: https://github.com/cbw29512/Tab5LASR/pull/8
