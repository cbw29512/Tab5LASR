# Review: 1 MiB to 2 MiB Tab5 factory partition for LASR web viewer

**Draft / not a flash authorization.**

## Grounded facts from the user's own read-only inspection

- Current full-flash size: 16,777,216 bytes (16 MiB), ESP32-P4 rev v1.3
- Full dice-roller backup (before LASR was flashed) was read twice and privately archived; original SHA-256 `52545123E6C20C4B139F290B64E4C70701C3618616D9A4149D444039CE7A3C01`
- Partition list recovered from that image:
  - `nvs`: data/nvs, `0x9000..0xF000`, 24 KiB
  - `phy_init`: data/phy, `0xF000..0x10000`, 4 KiB
  - `factory`: app/factory, `0x10000..0x110000`, 1 MiB
- Latest compiled ESP32-P4 onboard CSI + JPEG + hosted-C6 Wi-Fi + HTTP exceeded original factory partition by 298,304 bytes (`0x48D40`); **CI blocked flash image due to actual size**.
- User physically confirmed existing installed standalone onboard camera image is upright and runs on battery.
- Color setting 1 is closest to expected, though a second observer reports a yellow cast. Do not alter color handling until independent calibration.

## Proposed candidate

Keep *exactly* the same NVS/PHY offsets, factory **start** `0x10000`, and expand only its size to `0x200000` (2 MiB). New factory end `0x210000`; proposed extra region is `0x110000..0x210000` (end exclusive).

See `onboard/partitions_web_2m.csv` and `tools/verify_proposed_partition.py` on this review branch. Its GitHub build should use a separate `sdkconfig.partition_2m_probe`, not production's existing partition table.

## Mandatory gates before any partition-table write

1. `main` read-only `tools/CHECK_TAB5_WEB_EXPANSION.ps1` compares two **physical-device** reads of the extra range and audits them against the verified original dice-roller image.
2. Abort if either original or current extra region has even **one** byte other than `FF`, or the two physical reads disagree. An all-FF region is necessary, **not sufficient** to prove it is free of vendor use.
3. Archive **another complete verified readback** of the *currently working* upright battery-powered CSI firmware, privately, before any layout migration. The original dice readback remains untouched.
4. Record/verify bootloader offset, firmware flash-encryption/security flags, original partition-table bytes and compiled new table's **only permitted difference** (factory size). Do not erase the NVS or PHY partitions.
5. Confirm the ESP32-C6 SDIO co-processor firmware is compatible with the ESP-Hosted host implementation. M5Stack's factory C6 firmware may be an older hosted protocol. **Do not automatically update/flash C6** without its own independent backup and recovery arrangement.
6. Candidate CSI + hosted Wi-Fi/JPEG/HTTP must pass CI, image revision v1.x check, 2 MiB custom table validation, and size check.
7. Only a separate reviewed, deliberately confirmed migration procedure may install a new partition table **and** associated application with a documented power-loss recovery path. No force/erase-flash commands.
8. After migration verify battery-only camera + Wi-Fi + browser, charger behavior, P4/P5/P7 state, battery recovery controls, FPS, stream age, and local network isolation.

## Current milestone

The browser interface tests pass and draft [PR #6](https://github.com/cbw29512/Tab5LASR/pull/6) holds the experimental device-side integration. A physical Wi-Fi viewing URL does not yet exist. **No new partition layout has been flashed.**

## User read-only command (Windows PowerShell 5.1)

```powershell
$ErrorActionPreference = 'Stop'
try {
  $project = Join-Path $HOME 'Tab5LASR'
  & git -C $project pull --ff-only
  if ($LASTEXITCODE -ne 0) { throw 'Project update failed' }
  & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
      (Join-Path $project 'tools\CHECK_TAB5_WEB_EXPANSION.ps1') -Port COM5
  if ($LASTEXITCODE -ne 0) { throw 'Read-only expansion review did not pass' }
  Write-Host 'Expansion region is erased in both snapshots; further safety gates remain.' -ForegroundColor Green
} catch {
  Write-Warning "STOP: $($_.Exception.Message)"
}
```

This command only reads flash and produces local samples. It doesn't modify bootloader, application, C6, or partition table.
