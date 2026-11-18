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