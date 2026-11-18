# First physical test: Tab5 built-in MIPI-CSI camera

**Only use the onboard CSI build.** The USB-UVC artifact in `firmware/` is not for this test.

## Verified image / backup

- Image: `tab5lasr_onboard.bin`, **946,240 bytes**; original 1 MiB factory app (1,048,576 B) at **0x10000**
- CI run [44206614851](https://github.com/cbw29512/Tab5LASR/actions/runs/44206614851), successful, commit `148ed1a029396604c4c2fa91b7bb73063556e536`
- [Download artifact](https://github.com/cbw29512/Tab5LASR/actions/runs/44206614851/artifacts/14384371506), name `tab5lasr-onboard-148ed1a029396604c4c2fa91b7bb73063556e536`
- Camera driver: direct Espressif 1.3.3 BSP `bsp_camera_start` + `esp_video` / V4L2 + PPA RGB565 preview
- The Tab5 source documentation says SC2356; current official BSP driver config is `CONFIG_CAMERA_SC202CS=y`. **Sensor success is not yet physically verified.**
- Private verified recovery release: `cbw29512/Tab5DiceRecovery` / `tab5-dice-readback-20261117-173241`
- Full flash readback SHA-256: `52545123E6C20C4B139F290B64E4C70701C3618616D9A4149D444039CE7A3C01`
- Power: factory power-off P4 LOW, IP2326 charger config and INA226 telemetry, guarded two-tap 30-second recovery UI ported. **Battery-only operation remains physically unverified.**

## Built-in camera 180-degree correction

User's first physical flash produced a live **upside-down** picture. In `onboard/main/onboard_ppa.c`, only the picture's rotation was corrected by +180 degrees, preserving screen/touch controls and all battery modules. The corrected CI build is **946,240 bytes**, ESP32-P4 v1.x compatible, 1 MiB factory partition safe.

**Recommended one-command installation** (PowerShell 5.1, project already cloned and permanent recovery verified):

```powershell
$ErrorActionPreference = 'Stop'
try {
    $project = Join-Path $HOME 'Tab5LASR'
    & git -C $project pull --ff-only
    if ($LASTEXITCODE -ne 0) { throw 'Could not update project' }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
        (Join-Path $project 'tools\INSTALL_ONBOARD_ROTATE180.ps1') -Port COM5
    if ($LASTEXITCODE -ne 0) { throw 'Camera rotation update did not complete' }
    Write-Host 'ONBOARD CAMERA 180-DEGREE FIX INSTALLED' -ForegroundColor Green
}
catch { Write-Warning "STOP: $($_.Exception.Message)" }
```

The helper refuses to flash unless GitHub confirms the exact corrected run succeeded and the original `FLASH_CAMERA_ONLY.ps1` passes all backup/partition/image/revision checks. No bootloader or partition sectors are written. Do not use the original upside-down artifact for this revision.

## Windows PowerShell 5.1 installation

1. Plug the Tab5 into your computer by USB-C (historically COM5) and leave the USB-A webcam unplugged.
2. Run from PowerShell. The **installer stops** if the local backup hash, factory partition offset/size, candidate application hash, or chip revision does not match.
3. **For Windows PowerShell 5.1**, pass `-ConfirmFlash 'FLASH CAMERA'` explicitly to avoid a nested-shell `Read-Host` receiving an empty answer. This is an intentional write authorization; omit the token if you do not intend to flash.

```powershell
$ErrorActionPreference = "Stop"
try {
    $project = Join-Path $HOME "Tab5LASR"
    $out = Join-Path $project "onboard-review"
    $backup = Join-Path $HOME "Tab5Recovery\20261117-173241"

    & git -C $project pull --ff-only
    if ($LASTEXITCODE -ne 0) { throw "Project update failed" }

    New-Item -ItemType Directory -Force -Path $out | Out-Null
    & gh run download 44206614851 `
        --repo cbw29512/Tab5LASR `
        --name "tab5lasr-onboard-148ed1a029396604c4c2fa91b7bb73063556e536" `
        --dir $out
    if ($LASTEXITCODE -ne 0) { throw "Onboard camera firmware download failed" }

    # Runs the guarded installer in a subprocess and explicitly authorizes flash.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File (Join-Path $project "tools\FLASH_CAMERA_ONLY.ps1") `
        -Port COM5 `
        -Firmware (Join-Path $out "tab5lasr_onboard.bin") `
        -BackupFolder $backup `
        -ConfirmFlash "FLASH CAMERA"
    if ($LASTEXITCODE -ne 0) { throw "Camera application installation stopped" }

    Write-Host "ONBOARD CSI CAMERA APP FLASHED" -ForegroundColor Green
} catch {
    Write-Warning "STOP: $($_.Exception.Message)"
}
```

**No bootloader or partition table is modified.** You can restore the full-device dice flash from the separately verified private backup if necessary, using a deliberate and independently verified recovery procedure.

## What should happen

The Tab5 should boot its camera-only firmware, initialize its built-in camera, and show the live onboard picture on the screen. The camera and screen are on the same face of the device; aim them toward a well-lit, nearby subject for the first check.

For logs (after the application flash finishes and the COM port is available):

```powershell
python -m serial.tools.miniterm COM5 115200
```

`Ctrl+]` closes the serial monitor. If the terminal connects but shows no output, briefly reboot the Tab5 while the monitor is open.

Expected serial messages include:
- `Onboard MIPI CSI only. USB webcam is not used.`
- `Sensor WIDTHxHEIGHT FOURCC=...`
- `MIPI CSI ... capture=XX.X FPS preview=YY.Y FPS errors=N`
- `INA226 battery bus: ...` and recurring `BAT ... CHG CMD ...`

Only **actual device logs** can establish true FPS, sensor detection and success. If the camera fails, capture the sensor-driver/CSI errors. Do not assume the screen being lit proves frames were received.

## Battery acceptance

Start with USB-C connected. Check battery bus/shunt and `CHG CMD` logs, then verify whether a charged NP-F550 continues powering the Tab5 **with camera preview running** when USB-C is unplugged. After that, test a cold boot on battery.

If battery voltage is abnormal or camera causes resets, stop testing and reconnect USB. Do not bypass charger safeguards or reconfigure the P4/P5/P7 power expander pins.

## Next

After the onboard camera passes its physical test, the next milestone is a local-network browser viewer. It will show that camera feed without needing a USB webcam. Avoid claiming streaming support before implementation.
