# Tab5 LASR: camera + microSD combined hardware acceptance

## Objective and definition of done

This **engineering** build tests the physical onboard CSI camera and the already
proven, no-format synthetic microSD session writer concurrently, without touching
the bootloader, partition map, Wi-Fi coprocesor, private recovery backups, or
existing saved sessions.

It is **not** a release-ready target scorer or live SIRT session recorder.

## Data schema and state

- `CONFIG_LASR_COMBINED_DIAGNOSTIC`: off by default. CI sets this only for the
  independently reviewed **combined** profile.
- `CONFIG_LASR_CSI_DIAGNOSTIC`: read-only raw camera blob/pulse candidates.
  These have timestamp and raw normalized X/Y, but are not calibrated shots.
- `LASR/diagnostics/synthetic-session-<id>/LASR/sessions/fake_shots/`:
  existing fixed 2-event UNSCORED fixture. These are fake tests, never laser data.
- Camera state: `CAMERA_INIT -> FORMAT_READY -> STREAMING`.
- Storage state: `NOT_STARTED -> TASK_STARTED -> MOUNTED -> WRITTEN ->
  UNMOUNTED -> REMOUNTED -> EXACT_BYTES_VERIFIED -> UNMOUNTED`, or FAILED.
- `PASS` on the SD widget means synthetic storage persistence only. Physical
  concurrent success additionally requires camera FPS and preview to continue
  **throughout** the storage operation.

## Logic flow (engineering profile only)

1. Display/charger/battery diagnostics start exactly as for normal camera.
2. The real CSI camera calls `STREAMON`, dequeues and displays real frames.
3. Once at least 4 frames have arrived, capture starts a **separate 24 KiB**
   SD task without waiting for filesystem I/O.
4. That task runs the previously physically proven no-format SD smoke and
   synthetic session; actual camera capture continues independently.
5. Capture optionally logs unverified bright-point pulse candidates in raw
   sensor coordinates. No candidate is written to any shooting session.
6. SD status UI displays PASS/FAIL. Neither status is a SIRT accuracy claim.

## Build gate

- Draft PR #12 targets the existing storage work in draft PR #11; do not merge.
- ESP-IDF 6.1 produces a **combined** review artifact with explicit
  `FLASH=NO_AUTO_FLASH` policy and image SHA-256. Strictly reject if it exceeds
  the original `0x100000` factory app partition.
- CI also verifies FAT long filenames, no SD-only boot mode, ESP32-P4 chip
  compatibility, Windows PowerShell 5.1 syntax, and read-only upgrade guards.
- The preflight may inspect the real device **read-only** against the verified
  last-installed SD diagnostic using the original private full-flash recovery.
  Only the user explicitly authorizes app-only installation with `FLASH`.

## Physical acceptance (not proven by CI)

- Confirm the *correct* COM port and original full-flash camera backup.
- Verify camera starts and preview/Color 2 remain readable, and CSI FPS is
  logged again after `CAMERA+SD TEST` begins.
- Verify SD task reports success and the display shows
  `microSD DIAGNOSTIC: PASS`; capture must continue after it completes.
- Power-cycle and use `tools/VERIFY_LASR_SD_RESULTS.ps1` on the removed
  card to inspect the preserved unique test files. Ensure old sessions remain.
- With an unloaded, safe SIRT training device, test repeated pulse, stationary
  bright light, and rearm behavior. **Only candidate logs** are expected.
- Explicitly validate battery-only, unplug/replug, camera fps and charging.
- USB-C file transfer, Wi-Fi file transfer, calibrated GO timing and true
  shot recording/scoring remain separate acceptance gates.

## Stop conditions

Missing SD, stack overflow, camera stall, corrupted/overwritten preexisting
files, build over 1 MiB, or a live-flash mismatch => stop. Never repartition,
auto-format, retry dangerous writes, or claim hardware completion.

## Physical result reported by operator (2026-11-18)

- Operator confirmed **live camera preview** with the combined-test
  `microSD PASS` panel overlay on the physical Tab5. This establishes
  both camera and SD can operate in the same installed build at least for
  the observed interval. Full FPS and post-SD endurance measurements were
  not supplied; do not silently certify them.
- The center overlay was subsequently relocated to a small bottom-right
  status. This UI change is repository-side only until separately flashed.
- A later revision adds an **atomic CSI frame-progress counter**, checking
  frames continue to arrive for 700ms *after* synthetic SD verification,
  before reporting combined PASS. Needs exact-head CI and physical retest.
- Safe upgrade comparison now accepts a previously installed, exact-hash
  verified `MODE=combined` artifact, rather than wrongly assuming the old
  SD-only image remains installed. Existing downloads can be reused after
  hash and live-flash verification.
- Raw-frame gap handling resets pulse state to WAIT_DARK (without inventing
  shots) and does not label normal dropped-frame rearming as sensor failure.
  Synthetic regression test included.

## Next physical SIRT probe acceptance

Use the review-only raw camera probe on the Tab5 *without any live recording*.
When safely testing an unloaded SIRT training device, observe USB serial logs
for `UNVERIFIED candidate` lines with monotonic timestamps and raw normalized
(x,y). If none appear, collect raw CSI format, capture FPS, lighting and
distance before tuning thresholds; no unmeasured sensitivity claims.

A visible pulse should produce **at most one** event until darkness re-arms.
A persistent lamp must not count as a sequence of shots. Report any capture
FPS slowdown, screen freeze, reboots or erroneous bright-room detections.
Do not feed these uncalibrated candidates into `lasr_recorder_task_shot`.
Save actual session data only after user-confirmed dark calibration,
true GO timing, coordinate alignment and source-backed scoring gates.

## Camera FPS / white-overlay hardware regression (2026-11-19)

Operator reported the combined build still displayed a white status box and
roughly 1 FPS live preview. The exact camera-frame timings were not supplied;
full-frame raw CSI scanning in the capture loop is a plausible bottleneck,
**not yet a measured root cause**.

The next opt-in combined test image removes the LVGL parent panel entirely.
A transparent text status `CSI + SD PASS` appears in the bottom-right; a
small bottom-left `LASER SCAN OFF` button permits a physical FPS A/B test.
The combined engineering build starts with scanning **OFF** to prioritize
camera preview. Tap the button to enable `LASER SCAN ON` while watching
preview FPS and USB serial logs; tap again to disable.

The raw CSI scanner now validates entire V4L2 frame metadata once before
its pixel loops, rather than for each pixel. The safe externally callable
bounded decoder is retained, and host tests compare both decoders for
RGB565 LE/BE and UYVY. Serial scanner diagnostics provide measured scan
microseconds and candidate counts every ten scanned frames.

**Acceptance**: both modes must retain a live preview; record measured
`capture` and `preview` FPS lines from `lasr_csi`, plus `scan_last_us`
and `worst_us` from `lasr_csi_probe` when ON. This confirms whether
the scanner or display/camera path is the bottleneck. The 1 MiB hard
partition gate and app-only recovery comparisons remain mandatory.

`SCAN OFF` does not detect any laser pulses, and `SCAN ON` still uses
uncalibrated thresholds. Neither mode writes actual SIRT shots.
