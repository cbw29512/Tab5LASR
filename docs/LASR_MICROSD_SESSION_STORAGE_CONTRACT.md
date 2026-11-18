# LASR microSD session storage contract (2026-11-18)

## Definition of Done

After a SIRT dry-fire session, the **Tab5 itself** saves the target, calibration identifier, ordered shot events, elapsed times, splits, hit zones and score totals to its **physical microSD card**, without a laptop, Wi-Fi, Internet or browser. The user can remove the card and inspect the results on a computer. On-device session viewing and reliable storage-state reporting are required. **No live hardware functionality is claimed by this document.**

## Hardware and library evidence

- The M5Stack Tab5 has an onboard microSD slot (M5Stack Tab5 product documentation).
- The official Espressif Tab5 BSP exposes `bsp_sdcard_mount()` / `bsp_sdcard_unmount()`, `bsp_sdcard_get_handle()`, and VFS-compatible `fopen`, `fwrite`, `fflush`, `fclose`. Compatibility must be verified against the repository's pinned BSP version before implementation.
- M5Stack's own Tab5 microSD example writes `WriteTest.txt`. These establish **hardware capability**, not that our current LASR firmware has a working filesystem.
- Sources: https://docs.m5stack.com/en/core/Tab5 ; https://github.com/espressif/esp-bsp/blob/master/bsp/m5stack_tab5/API.md ; https://docs.m5stack.com/en/arduino/m5tab5/microsd

## Schema and file structure (proposed v1)

```text
/LASR/sessions/<session-id>/
  session.json        # finalized summary and evidence of saved state
  shots.csv           # per-shot data, spreadsheet compatible
  target.json         # template ID and calibration snapshot
  shots.pending       # recoverable append journal; delete only after verification
```

`SessionHeaderV1 {schema_version:1, session_id:string, device_id_local:string?, target_id:string, calibration_revision:number, started_utc:string|null, started_boot_ms:number, start_delay_ms:number, status:'recording'|'paused'|'complete'|'incomplete', shot_count:number, score_total:number, save_state:'pending'|'saved'|'error'}`.

`ShotRecordV1 {sequence:number, elapsed_ms:number, split_ms:number, x_norm:number|null, y_norm:number|null, zone:string|null, score:number|null, source:'camera', confidence:number|null}`.

Timing rules: timestamps are relative to the **actual GO signal** and based on a monotonic device timer. Pauses and resume-countdown durations do not contribute to elapsed shot time. Real wall-clock UTC may be `null` when the portable unit has no trusted time source. Never use an arbitrary wall-clock date as factual metadata.

Camera coordinates and scoring zones derive from the **raw camera detection path**, not Color 2/4 display correction. No camera video is required for v1; video capture remains a separate measured performance milestone.

## State machine

`NO_CARD -> MOUNTING -> READY -> RECORDING -> FINALIZING -> SAVED`.

- `NO_CARD`: show **SD card missing; session will not be persistent**. User can still start a volatile session if explicitly acknowledged.
- `MOUNTING`: initialize card and FAT filesystem; never set automatic format-on-mount-failure.
- `READY`: verify writable directory and available space before reporting ready.
- `RECORDING`: a bounded async writer queue receives confirmed shot events; sensor/timer must never wait for a slow card.
- `FINALIZING`: flush/close and re-open the files to verify expected record count, summary and length before showing SAVED.
- `SAVED`: only after actual storage verification; report session folder location on screen.
- `ERROR`: report card full, removal, mount, write, flush and validation errors visibly. Preserve in-memory shot log while power remains; never claim persistence.
- `INCOMPLETE`: on next boot, enumerate recoverable pending journals; offer to recover safely rather than silently discard partial sessions.

## Reliability / power-loss policy

1. A single owner opens/closes the microSD device. Preallocate or budget storage where feasible; no dynamic allocations on the camera capture path.
2. Create a **new unique** session directory, write metadata to a temporary file, and verify writes; do not overwrite previous sessions.
3. Append each confirmed shot to a bounded journal and flush via a separate writer task; decide flush cadence from actual measured worst-case SD latency. Timestamp the shot *before* queuing it.
4. Write final `shots.csv` and `session.json` to temporary names, flush and close them, rename after verification. FAT file rename is not a guaranteed power-fail-atomic transaction; keep the recoverable journal.
5. On missing or failed card, keep session functional if safe, but screen must clearly show **UNSAVED**; never auto-format a user's card.
6. Test SD card mounting alongside camera, touch display, battery recovery and optional ESP32-C6 Wi-Fi. Confirm card power circuitry and resource conflicts from pinned Tab5 BSP/board schematic.
7. Never embed secret Wi-Fi credentials, private firmware backups, raw device flash, or account data in session export.
8. No changes to bootloader, flash partitions, P4/P5/P7 charger safety or active Color 2/4 profile without separate acceptance.

## Acceptance tests required before claiming microSD feature works

- Real Tab5 mounted with microSD: write + close + read back a session directory, CSV and JSON.
- 25+ timed shot events: verify exact ordered event count and monotonic times; compare timestamps with display.
- Remove card / full card / read-only or malformed FAT / slow card: no camera deadlock, no false SAVED indication.
- Interrupt power while writing: recover last journal and mark incomplete with preserved shots; no previous-session corruption.
- Boot without network: record and review locally; power cycle and reopen saved files.
- Test simultaneously on battery with live CSI camera and (when feasible) ESP32-C6 WLAN; benchmark dropped frames and writer latency.
- Run parser/export/schema unit tests in CI, plus on-hardware acceptance; CI success alone does not validate physical storage.

## Implementation notes

Use the exact compatible version of the Tab5 ESP-IDF BSP (`bsp_sdcard_mount`) after checking the repository's pinned `idf_component.yml`; if ABI/version differs, **verify docs before coding**. Split capture, event model, writer queue, SD HAL and UI into separate modules (under 150 lines each where practical). Each C entrypoint needs explicit failure returns and `ESP_LOGE/W` diagnostics. The existing browser MVP's localStorage is *not* microSD, and browser-shot CSV export is a separate mechanism.

## Status

**Design documented; no LASR firmware microSD writer built or tested yet.** Development stays on a review branch. No device flashing authorized here.
