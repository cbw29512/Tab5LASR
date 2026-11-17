# First physical Tab5 camera capability test

## Baseline

The first camera-only ESP32-P4 image passed GitHub Actions run [44186948575](https://github.com/cbw29512/Tab5LASR/actions/runs/44186948575).

- GitHub source commit: `acea11d250821704333a747f9ae555b946b17149`
- Application image: `tab5lasr_camera.bin`, 452,256 bytes
- Image revision metadata: minimum v1.0 / maximum v1.99, compatible with known v1.3
- Host camera FPS tests: passed
- Webcam and real Tab5 screen: **not physically tested**
- Disposable artifact: [GitHub Actions download](https://github.com/cbw29512/Tab5LASR/actions/runs/44186948575/artifacts/14374987749) (expires 2026-12-01)

## Safe order (Windows PowerShell 5.1)

1. From your normal Windows terminal, create a **private** backup repository once:
   `gh repo create cbw29512/Tab5DiceRecovery --private --add-readme`
   If it already exists, do not recreate it. Verify with `gh repo view cbw29512/Tab5DiceRecovery --json visibility` and confirm `PRIVATE`.
2. Clone this project: `gh repo clone cbw29512/Tab5LASR "$HOME\Tab5LASR"`. If you already have a clone, `git -C "$HOME\Tab5LASR" pull --ff-only` instead.
3. Connect your working Tab5 by USB-C, confirm the correct port in Device Manager (historically `COM5`).
4. Run the read-only, permanent backup with a verified redownload:
   `& "$HOME\Tab5LASR\tools\BACKUP_TAB5_DICE.ps1" -Port COM5 -PrivateRepo cbw29512/Tab5DiceRecovery`
5. **Confirm** `PERMANENT PRIVATE RELEASE VERIFIED`. Keep the printed timestamped directory under `$HOME\Tab5Recovery`. If the backup fails, **STOP**.
6. Download the review application-only image:
   `gh run download 44186948575 --repo cbw29512/Tab5LASR --name tab5lasr-camera-review-acea11d250821704333a747f9ae555b946b17149 --dir "$HOME\Tab5LASR\camera-review"`
7. Run the gated application-only installer, substituting the exact timestamped backup directory:
   `& "$HOME\Tab5LASR\tools\FLASH_CAMERA_ONLY.ps1" -Port COM5 -Firmware "$HOME\Tab5LASR\camera-review\tab5lasr_camera.bin" -BackupFolder "$HOME\Tab5Recovery\YYYYMMDD-HHMMSS"`
8. Review the original factory partition and firmware hash checks, then explicitly type `FLASH CAMERA`. This writes **only the application at 0x10000**, not bootloader or partition table.

## Hardware acceptance

- Plug a UVC webcam into the Tab5 USB-A port and use USB-C for serial logs.
- Verify live 320×240 picture on the Tab5 and logs showing actual captured/preview FPS every 5 seconds.
- Test unplug and reconnect with a live camera; record VID:PID and advertised MJPEG modes.
- Confirm charger behavior and safe USB-to-battery handoff.
- A blank screen with no camera is **not** a successful test; check serial logs and camera format support.

**Only after these observations** should M2 Wi-Fi browser viewing be developed or higher requested FPS advertised.
