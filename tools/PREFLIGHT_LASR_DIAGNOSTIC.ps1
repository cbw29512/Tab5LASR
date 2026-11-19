# Windows PowerShell 5.1. READ-ONLY unless -Flash AND manual confirmation.
# Requires a CURRENT working-camera full 16 MiB private verified backup.
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [Parameter(Mandatory=$true)][string]$CandidateFolder,
    [Parameter(Mandatory=$true)][string]$CameraBackupFolder,
    [ValidateSet('sd-smoke','raw-csi')][string]$Mode = 'sd-smoke',
    # Optional: ONLY the previously flashed, CI-verified SD diagnostic image.
    # Required when the Tab5 is no longer running the camera backup.
    [string]$InstalledDiagnosticFolder = '',
    [switch]$Flash
)
$ErrorActionPreference = 'Stop'
$liveImage = $null
try {
    $candidate = (Resolve-Path -LiteralPath $CandidateFolder).Path
    $backup = (Resolve-Path -LiteralPath $CameraBackupFolder).Path
    $image = Join-Path $candidate 'tab5lasr_onboard.bin'
    $sums = Join-Path $candidate 'SHA256SUMS.txt'
    $policyPath = Join-Path $candidate 'FLASH_POLICY.txt'
    $recovery = Join-Path $backup 'tab5-lasr-camera-full-verified.bin'
    $manifestPath = Join-Path $backup 'SHA256-MANIFEST.json'
    $markerPath = Join-Path $backup 'PRIVATE_RELEASE_VERIFIED.txt'
    $previousApp = $null
    if ($InstalledDiagnosticFolder) {
        $previousFolder = (Resolve-Path -LiteralPath $InstalledDiagnosticFolder).Path
        $previousApp = Join-Path $previousFolder 'tab5lasr_onboard.bin'
        $previousSums = Join-Path $previousFolder 'SHA256SUMS.txt'
        $previousPolicy = Join-Path $previousFolder 'FLASH_POLICY.txt'
        foreach ($p in @($previousApp,$previousSums,$previousPolicy)) {
            if (!(Test-Path -LiteralPath $p -PathType Leaf)) {
                throw "Previous diagnostic evidence missing: $p"
            }
        }
        $previousExpected = (((Get-Content -LiteralPath $previousSums |
            Select-Object -First 1) -split '\s+')[0]).ToUpperInvariant()
        $previousHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $previousApp).Hash.ToUpperInvariant()
        if ($previousExpected.Length -ne 64 -or $previousExpected -cne $previousHash) {
            throw 'Installed diagnostic reference has invalid SHA-256 evidence'
        }
        $oldPolicy = Get-Content -LiteralPath $previousPolicy -Raw
        if ($oldPolicy -notmatch '(?m)^MODE=sd-smoke\r?$' -or
            $oldPolicy -notmatch '(?m)^APP_OFFSET=0x10000\r?$' -or
            $oldPolicy -notmatch '(?m)^FLASH=NO_AUTO_FLASH\r?$') {
            throw 'Previous diagnostic policy is incompatible'
        }
    }
    foreach ($p in @($image,$sums,$policyPath,$recovery,$manifestPath,$markerPath)) {
        if (!(Test-Path -LiteralPath $p -PathType Leaf)) { throw "Required file missing: $p" }
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $marker = Get-Content -LiteralPath $markerPath -Raw
    if ($manifest.filename -cne 'tab5-lasr-camera-full-verified.bin' -or
        (Get-Item -LiteralPath $recovery).Length -ne 16777216) {
        throw 'Only a verified CURRENT CAMERA full-flash backup is accepted.'
    }
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $recovery).Hash.ToUpperInvariant()
    if ($backupHash -cne $manifest.sha256.ToUpperInvariant() -or
        $marker -notmatch [regex]::Escape($backupHash)) { throw 'Private recovery SHA-256 evidence mismatch' }
    if (!$manifest.source_read_hashes -or $manifest.source_read_hashes.Count -ne 2 -or
        $manifest.source_read_hashes[0] -cne $backupHash -or
        $manifest.source_read_hashes[1] -cne $backupHash) {
        throw 'Original pair of physical flash readback hashes not verified'
    }
    $policy = Get-Content -LiteralPath $policyPath -Raw
    if ($policy -notmatch "(?m)^MODE=$([regex]::Escape($Mode))\r?$" -or
        $policy -notmatch '(?m)^FLASH=NO_AUTO_FLASH\r?$' -or
        $policy -notmatch '(?m)^APP_OFFSET=0x10000\r?$' -or
        $policy -notmatch '(?m)^MAX_PARTITION=1048576\r?$') {
        throw 'Diagnostic mode or review-only flash policy mismatch'
    }
    $first = (Get-Content -LiteralPath $sums | Select-Object -First 1)
    $expected = (($first -split '\s+')[0]).ToUpperInvariant()
    $imageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $image).Hash.ToUpperInvariant()
    if ($expected -cne $imageHash -or $expected.Length -ne 64) {
        throw 'Diagnostic application SHA-256 mismatch'
    }
    $info = & python -m esptool --chip esp32p4 image-info $image
    if ($LASTEXITCODE -ne 0) { throw 'Image is not a verified ESP32-P4 application' }
    $imageInfo = $info -join "`n"
    if ($imageInfo -notmatch 'Minimal chip revision: v1\.0' -or
        $imageInfo -notmatch 'Maximal chip revision: v1\.99') {
        throw 'Firmware image revision is incompatible with verified ESP32-P4 v1.3'
    }
    $gate = Join-Path $PSScriptRoot 'verify_factory_partition.py'
    if (!(Test-Path -LiteralPath $gate -PathType Leaf)) { throw 'Factory partition verifier missing' }
    & python $gate $recovery $image
    if ($LASTEXITCODE -ne 0) { throw 'Original factory app partition validation failed' }
    # Verify actual connected flash equals the archived current-camera backup.
    # This read-only step catches a changed device, backup or COM-port selection.
    $liveImage = Join-Path ([IO.Path]::GetTempPath()) ("lasr-preflight-" + [guid]::NewGuid().ToString('N') + '.bin')
    & python -m esptool --chip esp32p4 --port $Port --baud 460800 `
        read-flash 0x0 0x1000000 $liveImage
    if ($LASTEXITCODE -ne 0 -or (Get-Item -LiteralPath $liveImage).Length -ne 16777216) {
        throw 'Read-only connected-Tab5 verification failed'
    }
    $liveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $liveImage).Hash.ToUpperInvariant()
    if ($liveHash -cne $backupHash) {
        if (!$previousApp) {
            throw 'LIVE FLASH DIFFERS FROM CAMERA BACKUP; supply the verified installed diagnostic for upgrade'
        }
        $upgradeGuard = Join-Path $PSScriptRoot 'verify_installed_sd_upgrade.py'
        if (!(Test-Path -LiteralPath $upgradeGuard -PathType Leaf)) {
            throw 'Installed diagnostic read-only verifier missing'
        }
        & python $upgradeGuard $recovery $liveImage $previousApp
        if ($LASTEXITCODE -ne 0) {
            throw 'Live Tab5 does not match the exact previous app-only SD diagnostic flash'
        }
        Write-Host 'PASS: live device matches prior verified SD diagnostic; non-app flash unchanged.' -ForegroundColor Green
    } else {
        if ($previousApp) {
            throw 'Live device still matches camera; do not supply InstalledDiagnosticFolder'
        }
        Write-Host 'PASS: live device matches the original camera recovery backup.' -ForegroundColor Green
    }
    Write-Host "Profile=$Mode. This is a diagnostic, NOT completed laser scoring or session recording." -ForegroundColor Yellow
    if (!$Flash) {
        Write-Host 'READ-ONLY PREFLIGHT COMPLETE. NO FLASH OCCURRED.' -ForegroundColor Green
        return
    }
    $confirm = Read-Host 'Type FLASH to replace the installed application at 0x10000'
    if ($confirm -cne 'FLASH') { throw 'Cancelled; no firmware written' }
    & python -m esptool --chip esp32p4 --port $Port --baud 460800 `
        write-flash 0x10000 $image
    if ($LASTEXITCODE -ne 0) { throw 'Application-only diagnostic flash failed' }
    Write-Host 'DIAGNOSTIC APP FLASHED; now validate camera, battery, serial logs and microSD in person.' -ForegroundColor Green
}
catch {
    Write-Warning "LASR DIAGNOSTIC BLOCKED: $($_.Exception.Message)"
    exit 1
}
finally {
    if ($liveImage -and (Test-Path -LiteralPath $liveImage -PathType Leaf)) {
        Remove-Item -LiteralPath $liveImage -Force -ErrorAction SilentlyContinue
    }
}