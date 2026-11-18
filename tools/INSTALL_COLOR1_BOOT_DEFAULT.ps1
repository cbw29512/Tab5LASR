# Install the camera-only build that boots in Color 1.
# Windows PowerShell 5.1. Only flashes the application at 0x10000.
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [string]$OriginalBackupFolder = "$HOME\Tab5Recovery\20261117-173241",
    [string]$CameraBackupFolder = "$HOME\Tab5Recovery\camera-working-20261117-212814",
    [string]$ConfirmFlash = ''
)
$ErrorActionPreference = 'Stop'
try {
    $repository = 'cbw29512/Tab5LASR'
    $commit = '554d59cb909a88431aa004f10bcfdfa530b5b895'
    $runId = 44228290195

    # Protect BOTH recovery images, including the actual working camera.
    $cameraImage = Join-Path $CameraBackupFolder 'tab5-lasr-camera-full-verified.bin'
    $cameraMarker = Join-Path $CameraBackupFolder 'PRIVATE_RELEASE_VERIFIED.txt'
    $cameraManifest = Join-Path $CameraBackupFolder 'SHA256-MANIFEST.json'
    foreach ($item in @($cameraImage, $cameraMarker, $cameraManifest)) {
        if (!(Test-Path -LiteralPath $item -PathType Leaf)) {
            throw "Working-camera backup missing: $item"
        }
    }
    $expected = 'C1695018BFE1B1871F2C468C606A47F1A18311BAC5C2D934DFBE1258E52F2773'
    $manifest = Get-Content -LiteralPath $cameraManifest -Raw | ConvertFrom-Json
    $actual = (Get-FileHash -LiteralPath $cameraImage -Algorithm SHA256).Hash.ToUpperInvariant()
    if ((Get-Item -LiteralPath $cameraImage).Length -ne 16777216 -or
        $actual -cne $expected -or $manifest.sha256 -cne $expected) {
        throw 'Working-camera backup verification failed'
    }

    # The exact source commit must have both host tests and P4 build green.
    $runData = & gh run view $runId --repo $repository --json status,conclusion,headSha | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or !$runData) {
        throw 'Cannot verify GitHub firmware build'
    }
    if ($runData.status -ne 'completed' -or $runData.conclusion -ne 'success' -or
        $runData.headSha -cne $commit) {
        throw 'Color 1 build is not green at the pinned revision'
    }

    $project = Split-Path -Parent $PSScriptRoot
    $folder = Join-Path $project "onboard-color1-554d59cb9"
    if (Test-Path -LiteralPath $folder) {
        throw "Refusing to reuse old artifact folder: $folder"
    }
    New-Item -ItemType Directory -Path $folder | Out-Null
    $artifact = "tab5lasr-onboard-$commit"
    & gh run download $runId --repo $repository --name $artifact --dir $folder
    if ($LASTEXITCODE -ne 0) {
        throw 'Verified camera artifact unavailable'
    }
    $image = Join-Path $folder 'tab5lasr_onboard.bin'
    if (!(Test-Path -LiteralPath $image -PathType Leaf)) {
        throw 'Camera image missing from downloaded artifact'
    }

    # The existing guarded installer also checks the original dice backup,
    # original partition offsets, firmware SHA256, size and chip revision.
    $installer = Join-Path $PSScriptRoot 'FLASH_CAMERA_ONLY.ps1'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -Port $Port -Firmware $image -BackupFolder $OriginalBackupFolder -ConfirmFlash $ConfirmFlash
    if ($LASTEXITCODE -ne 0) {
        throw 'Application-only camera installation failed or was cancelled'
    }
    Write-Host 'COLOR 1 STARTUP DEFAULT INSTALLED.' -ForegroundColor Green
    Write-Host 'Verify display, orientation, and battery-only operation.' -ForegroundColor Green
}
catch {
    Write-Warning "CAMERA INSTALL STOPPED: $($_.Exception.Message)"
    exit 1
}
