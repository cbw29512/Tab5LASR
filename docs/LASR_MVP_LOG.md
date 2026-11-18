# Tab5 LASR — MVP engineering log

**2026-11-18 — Target template and camera shot timer browser MVP**

## Definition of Done — browser MVP

- Five recognizable **original illustrative** training target templates.
- Pick target, adjust four alignment handles, store alignment for each target locally.
- Saved geometry restores on browser restart, but alignment must be confirmed again.
- Start runs audible 3/5/10-second countdown, then begins monotonic elapsed clock at GO.
- Pause freezes accumulated time; resume repeats countdown; stop cancels pending audio.
- Manual demo-shot injection produces elapsed/split time, score zone and point total.
- Export CSV of simulated shots, usable offline, without external services.
- Browser tests for states, persistence, template switches, countdown, shot splits, and termination.
- No firmware, partition, charger, camera source, or Wi-Fi firmware changes.

## Browser state schema (v1)

`Settings { schemaVersion: 1, activeTemplateId, profiles: Record<templateId,{corners: [4][2],savedAt}> }`.
Each corner is in preview SVG coordinates (760×410); this is **browser-only** prototype data.
`Session {phase: CHECK_ALIGNMENT|READY|COUNTDOWN|ACTIVE|PAUSED|COMPLETE,
alignmentConfirmed, shots[], accumulatedMs,runStartedAtMs,countdownEndsAtMs}`.
`Shot {number,x,y,u,v,zone,score,elapsedMs,splitMs,source:"demo"}`.

## Explicitly deferred

- On-device NVS persistence with atomic revisions and camera fingerprints.
- Live calibrated onboard CSI frames and any real SIRT laser detection.
- Homography and physical-size scaling; browser demonstration uses a bilinear approximation.
- Device microSD file saving and battery-only session testing.
- Final physical camera relocation/optical redirect.
- Physical evidence of detection latency, false-positive rate, sensitivity, hit accuracy and battery runtime.

## Existing hardware blockers (not changed)

- [PR #6](https://github.com/cbw29512/Tab5LASR/pull/6) is draft: experimental MJPEG + hosted C6 Wi-Fi firmware overflowed the existing 1 MiB factory app partition. **Do not flash** or force partition changes.
- Current camera is same-side as screen; rear-facing optical/mounting design remains unproven.
- Preserve previously verified camera/dice-roller full-flash backups and the known-good battery/recovery controls.
- Color 2/4 is only a preview preference, never a raw detection transform.

## Build path beyond browser MVP

1. Approve browser UX and test manual alignment and timer behavior.
2. Verify on-device camera/laser frame path and exact layout before integrating target geometry.
3. Implement universal calibrated transform and raw-frame shot detection.
4. Design NVS calibration slots/CRC and microSD session records after verifying available partitions.
5. Run offline hardware acceptance and then optional web camera viewer integration.

## Validation record

Local browser interaction tests and syntax checks are recorded in the PR description and should be rerun on every change. No physical Tab5 acceptance is claimed.

## GitHub CI verification (2026-11-18)

- Local Node test suite: **7 passed, 0 failed**; source syntax checks passed.
- GitHub Actions workflow: [successful run 44264568231](https://github.com/cbw29512/Tab5LASR/actions/runs/44264568231).
- A second [successful run 44264516169](https://github.com/cbw29512/Tab5LASR/actions/runs/44264516169) also completed.
- This verifies the automated model/timer workflow, **not** live browser click testing or real Tab5 hardware operation. Browser automation in the current execution environment was blocked at page navigation and remains pending on a normal browser.

## Local microSD session recording requirement (2026-11-18)

- User requested every physical Tab5 shooting session saved **on the device's microSD card**. This is a required standalone feature, not a browser download dependency.
- Hardware feasibility confirmed through official M5Stack microSD read/write example and Espressif Tab5 BSP SD mount + VFS API.
- Session v1 will store target/calibration metadata, elapsed and split shot times, normalized impact coordinates, zones, scores and local save status as `session.json`, `target.json` and `shots.csv`, with a crash-recovery journal.
- **No on-device session writer exists yet.** The browser MVP's `localStorage` and CSV demo export must not be represented as microSD persistence.
- Power loss, card removal/full/corrupt card, offline running, camera+SD concurrent load and **visible UNSAVED warnings** are mandatory tests.
- Camera video recording is explicitly *not* part of v1; investigate later after measured performance and storage throughput.
- Detailed contract: [LASR_MICROSD_SESSION_STORAGE_CONTRACT.md](LASR_MICROSD_SESSION_STORAGE_CONTRACT.md).

## Wi-Fi and USB-C transfer request (2026-11-18)

- User wants completed **microSD session files moved/copied to a laptop via Wi-Fi OR USB-C cable**, without removing the card. Copy is default; originals remain on the Tab5.
- Hardware supports both in principle: onboard C6 Wi-Fi, Tab5 USB-C OTG, Espressif TinyUSB SD/MMC MSC support for ESP32-P4. Neither path is **implemented or validated on the current LASR firmware**.
- Wi-Fi target: local authenticated session list with CSV/ZIP download, no cloud/Internet. Existing ESP-Hosted/web firmware size + C6 compatibility blocker remains; smaller download-only firmware is an investigation, not a proven fix.
- USB-C target: explicit transfer mode that finalizes session, unmounts FAT from Tab5, exports SD through USB MSC; Windows eject required before Tab5 remounts. Never concurrently write SD from Windows and Tab5.
- Require visible transfer state, file verification, no silent formatting, no automatic deletion, no power or firmware safety regressions.
- Implementation/safety contract: [LASR_SESSION_TRANSFER_CONTRACT.md](LASR_SESSION_TRANSFER_CONTRACT.md).

## Embedded storage implementation started (2026-11-18)

- [Firmware PR #11](https://github.com/cbw29512/Tab5LASR/pull/11) now contains portable C source for recoverable shot-session journaling, verified CSV/JSON files, exclusive USB media ownership states, and explicit **non-formatting** Tab5 SDMMC adapter using BSP v1.3.3.
- [Storage CI run 44314432306](https://github.com/cbw29512/Tab5LASR/actions/runs/44314432306) passed; ESP32-P4 application build/1 MiB gate in [run 44314432151](https://github.com/cbw29512/Tab5LASR/actions/runs/44314432151) requires its own result.
- **No physical microSD recording yet**; code is compiled but not started or connected to laser events. Do not claim USB/Wi-Fi transfers are live. No flash operations have occurred.
