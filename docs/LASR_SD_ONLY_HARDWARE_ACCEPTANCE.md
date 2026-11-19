# Tab5 LASR SD-only first hardware acceptance (review branch)

## Objective and safety

Verify **actual** microSD I/O and complete session-format persistence on the physical Tab5, without starting camera capture or touching partitions or battery safeguards. This is an engineering-only image. The installed camera image stays untouched until a user-authorized, recovery-verified application-only flash. The diagnostic MUST fit the installed original 1 MiB factory app partition. No auto-format on mount failure.

## Data and states

- `LASR/diagnostics/lasr-smoke-<id>.txt`: temporary file written, read back, then deleted **only after successful verification**.
- `LASR/diagnostics/synthetic-session-<id>/LASR/sessions/fake_shots`: persistent synthetic `shots.pending`, `shots.csv`, `session.json`, `target.json`. Kept on the card for computer inspection; does not pollute `/LASR/sessions` (which will hold future real sessions).
- `synthetic_test` and `UNSCORED` indicate generated diagnostic values; 2 deliberately invented timestamps (150 ms and 410 ms) exercise the session writer but are **not** claimed as measured camera shots.
- On-screen PASS only if temporary file readback, synthetic session write/verification, and SD unmount all succeed. Any error shows FAILED, retains a partial journal when possible, and does not start the camera.

## Logic flow

`display + battery safety initialized -> mount(no_format) -> temp write/readback -> unique diagnostic directory -> session writer begin -> write 2 synthetic events -> finalize CSV/JSON -> readback -> unmount -> on-screen PASS/FAIL`.

## Verification

- Host: `test_lasr_sd_session_probe.c` verifies generated files, CSV/JSON and pending journal, repeated ID rejection, and safe path rejection. This does **not** test physical storage.
- GitHub: build **sd-smoke** independently and inspect the exact-head workflow, binary SHA-256 and image size. Normal camera and raw-CSI variants have separate CI gates.
- Physical: first read-only recovery preflight; on authorized flash, observe PASS/FAIL display and serial log, power cycle, remove SD card and read the synthetic CSV/JSON on Windows. Note whether an old session was altered (it must not be). Recheck battery and charging.
- Keep flash data and private recovery assets out of this public repository. Never blindly run the older dice-roller-only flash installer.

## Still NOT proven

Live CSI+SD concurrent use, camera/SIRT pulse sensitivity, calibrated shooting sessions, USB removable drive file transfer, and Wi-Fi download remain separate work. The synthetic file must never appear in live scoreboards.

## Download and safe device preflight (Windows PowerShell 5.1)

**Do not run the flash path until the exact selected diagnostic CI run has succeeded, the currently installed camera's full 16 MiB recovery backup and SHA-256 marker are available, and the correct COM port is confirmed.** This procedure changes no firmware unless `-Flash` is explicitly supplied and a further typed confirmation is given.

From a clone of the Tab5LASR review branch, use the helper `tools/PREPARE_LASR_SD_TEST.ps1`. Supply the exact **successful** GitHub diagnostic run ID and the actual folder containing the current-camera backup; do not use the older dice-roller backup. Example syntax:

```powershell
& .\tools\PREPARE_LASR_SD_TEST.ps1 -RunId <VERIFIED_RUN_ID> `
    -CameraBackupFolder '<YOUR_CURRENT_CAMERA_BACKUP_FOLDER>' -Port COM5
```

Without `-Flash` the helper downloads the SD-only artifact, checks the CI run and delegates to the read-only preflight. That preflight reads the connected Tab5's full flash to compare it with the verified **current-camera backup** and checks the candidate's SHA-256, chip revision, original factory partition and size. It intentionally rejects a mismatched live device. The artifact itself expires and does not substitute for the private full-flash recovery backup.

After inspecting the verified test plan and checking that your current camera firmware can be restored, `-Flash` can be supplied to the **same helper** for a deliberate application-only install. Even then, the preflight requires typing `FLASH` and writes **only** at `0x10000`; it never erases or repartitions the Tab5.

After an authorized SD-only firmware install, the camera will be disabled by design. Look for on-screen `microSD DIAGNOSTIC: PASS` or `FAILED`, review the USB serial logs, then inspect the uniquely named `LASR/diagnostics/synthetic-session-...` folder using the microSD card on a computer. Do not mistake the synthetic test data for laser shots or silently restore the camera without a validated restore procedure.

## Remount persistence verification and Windows inspection

The SD-only engineering diagnostic now unmounts the microSD filesystem after writing its synthetic session, remounts it with auto-formatting disabled, and independently reads all four expected files byte-for-byte. It unmounts again before showing PASS. The post-remount check detects missing files, unexpected extra rows, modified metadata, and inconsistent journal/CSV content. Physical power-cycle persistence and card removal still require hands-on testing.

After a safely completed diagnostic, remove the card and attach it to Windows. From PowerShell 5.1 on the LASR review branch, run the read-only verifier (replace E: with the mounted card):

    & .\tools\VERIFY_LASR_SD_RESULTS.ps1 -CardRoot 'E:\'

The script verifies exactly two UNSCORED fake events and rejects missing/corrupted CSV, journal, or JSON. It does not write, delete, format, or alter the card. Windows CI includes a passing fixture and a deliberately corrupted fixture that must fail. Neither check certifies real SIRT laser shots or concurrent camera and SD recording.
