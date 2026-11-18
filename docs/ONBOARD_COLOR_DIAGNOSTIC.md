# Built-in camera color test — visual comparison

## What the user's photo established

The Tab5 is oriented upright and functions on battery. The live image has an extreme neon-green/cyan cast; the neighboring phone display in the same photo looks normal. This does NOT conclusively distinguish RGB565 byte order, red-blue order, or camera YUV/sensor conversion.

## Diagnostic UI

An overlay at bottom left has a large COLOR mode button and three display-generated reference patches marked R, G, and B. The reference patches do not use the CSI camera or the PPA image conversion; they isolate the LCD UI color path.

| Button | Changes to CAMERA preview pixels |
| --- | --- |
| COLOR 1/4: ORIGINAL | No additional transform |
| COLOR 2/4: SWAP RED/BLUE | Swap five-bit RGB565 red and blue fields |
| COLOR 3/4: SWAP BYTES | Reverse the two bytes in each RGB565 pixel |
| COLOR 4/4: SWAP BOTH | Apply both transforms |

## Photography test

The user does not need to identify colors. Point the Tab5's built-in camera at a well-lit multicolored object. Capture a photo of the Tab5 for each numbered mode, with the image, mode label and R/G/B patches visible. Keep the camera pointing at the same object. Compare the four images and use the result to implement a permanent path; remove the diagnostic control afterward.

## Verified build

- Successful CI: https://github.com/cbw29512/Tab5LASR/actions/runs/44208623788
- Artifact: https://github.com/cbw29512/Tab5LASR/actions/runs/44208623788/artifacts/14383849716
- Application image: 948,336 bytes (under installed 1,048,576-byte partition).
- ESP32-P4 image revision: v1.0–v1.99, including the user's v1.3.
- FPS, battery policy, color-transform and backup partition parser tests: passed.

## Safety and continuity

- Do not rotate the screen or sensor: the working +180-degree camera preview orientation is preserved.
- No changes to the working power latch, battery charging, INA226 telemetry or guarded recovery UI.
- This changes only the PPA-produced RGB565 preview copy in PSRAM; never the requeued V4L2 capture buffer.
- GitHub Actions host tests now verify color transforms, original FPS calculations, battery policy and factory-partition checks.
- Installer: tools/INSTALL_ONBOARD_COLOR_TEST.ps1. It checks a successful exact GitHub Actions build, then uses the existing safety-gated application-only flasher.
- Original full 16MiB dice roller recovery image is retained in the private GitHub Release and locally under the dated Tab5Recovery folder.

## Windows install command (only after green build)

```powershell
$ErrorActionPreference = 'Stop'
try {
    $repo = Join-Path $HOME 'Tab5LASR'
    & git -C $repo pull --ff-only
    if ($LASTEXITCODE -ne 0) { throw 'Project update failed' }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\INSTALL_ONBOARD_COLOR_TEST.ps1') -Port COM5
    if ($LASTEXITCODE -ne 0) { throw 'Safe color-test firmware installation failed' }
    Write-Host 'COLOR DIAGNOSTIC INSTALLED' -ForegroundColor Green
}
catch { Write-Warning "STOP: $($_.Exception.Message)" }
```
