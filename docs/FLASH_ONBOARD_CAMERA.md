# First physical test: Tab5 built-in MIPI-CSI camera

**Only use the onboard CSI build.** The USB-UVC artifact in `firmware/` is not for this test.

## Verified image / backup

- Image: `tab5lasr_onboard.bin`, **946,208 bytes**; original 1 MiB factory app (1,048,576 B) at **0x10000**
- CI run [44193728872](https://github.com/cbw29512/Tab5LASR/actions/runs/44193728872), successful, commit `ae6f68346302e3ba306b63015d89ccc362ec3eac`
- [Download artifact](https://github.com/cbw29512/Tab5LASR/actions/runs/44193728872/artifacts/14377144105), name `tab5lasr-onboard-ae6f68346302e3ba306b63015d89ccc362ec3eac`
- Camera driver: direct Espressif 1.3.3 BSP `bsp_camera_start` + `esp_video` / V4L2 + PPA RGB565 preview
- The Tab5 source documentation says SC2356; current official BSP driver config is `CONFIG_CAMERA_SC202CS=y`. **Sensor success is not yet physically verified.**
- Private verified recovery release: `cbw29512/Tab5DiceRecovery` / `tab5-dice-readback-20261117-173241`
- Full flash readback SHA-256: `52545123E6C20C4B139F290B64E4C70701C3618616D9A4149D444039CE7A3C01`
- Power: factory power-off P4 LOW, IP2326 charger config and INA226 telemetry, guarded two-tap 30-second recovery UI ported. **Battery-only operation remains physically unverified.**

## Windows PowerShell 5.1 installation

1. Plug the Tab5 into your computer by USB-C (historically COM5) and leave the USB-A webcam unplugged.
2. Run from PowerShell. The **installer stops** if the local backup hash, factory partition offset/size, candidate application hash, or chip revision does not match.
3. Type `FLASH CAMERA` **exactly** when explicitly prompted; pressing Enter on a blank prompt cancels without flashing.

```powershell
$ErrorActionPreference = "Stop"
try {
    $project = Join-Path $HOME "Tab5LASR"
    $out = Join-Path $project "onboard-review"
    $backup = Join-Path $HOME "Tab5Recovery\20261117-173241"

    & git -C $project pull --ff-only
    if ($LASTEXITCODE -ne 0) { throw "Project update failed" }

    New-Item -ItemType Directory -Force -Path $out | Out-Null
    & gh run download 44193728872 `
        --repo cbw29512/Tab5LASR `
        --name "tab5lasr-onboard-ae6f68346302e3ba306b63015d89ccc362ec3eac" `
        --dir $out
    if ($LASTEXITCODE -ne 0) { throw "Onboard camera firmware download failed" }

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File (Join-Path $project "tools\FLASH_CAMERA_ONLY.ps1") `
        -Port COM5 `
        -Firmware (Join-Path $out "tab5lasr_onboard.bin") `
        -BackupFolder $backup
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
