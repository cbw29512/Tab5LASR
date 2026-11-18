# Installs the known 180-degree onboard camera fix after GitHub verifies it.
# Run from Windows PowerShell 5.1. Only the ESP32-P4 app at 0x10000 is replaced.
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [string]$BackupFolder = "$HOME\Tab5Recovery\20261117-173241"
)
$ErrorActionPreference = 'Stop'
try {
    $repo = 'cbw29512/Tab5LASR'
    $runId = 44206614851
    $artifact = 'tab5lasr-onboard-148ed1a029396604c4c2fa91b7bb73063556e536'
    $project = Split-Path -Parent $PSScriptRoot
    $output = Join-Path $project 'onboard-rotate180'

    # Require the exact revision's CI to have completed successfully.
    $run = & gh run view $runId --repo $repo --json status,conclusion,headSha |
        ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or !$run) {
        throw 'Could not read the onboard camera build status'
    }
    if ($run.status -ne 'completed' -or $run.conclusion -ne 'success') {
        throw "Build not green: $($run.status)/$($run.conclusion). No flash performed."
    }
    if ($run.headSha -ne '148ed1a029396604c4c2fa91b7bb73063556e536') {
        throw 'GitHub Actions build commit does not match rotation fix'
    }

    # Download to a distinct directory, avoiding mixing the previous image.
    if (Test-Path -LiteralPath $output) {
        throw "Directory already exists: $output. Inspect/remove it first to avoid stale files."
    }
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    & gh run download $runId --repo $repo --name $artifact --dir $output
    if ($LASTEXITCODE -ne 0) { throw 'GitHub firmware artifact download failed' }

    $candidate = Join-Path $output 'tab5lasr_onboard.bin'
    $installer = Join-Path $PSScriptRoot 'FLASH_CAMERA_ONLY.ps1'
    if (!(Test-Path -LiteralPath $candidate)) {
        throw 'Downloaded artifact does not contain tab5lasr_onboard.bin'
    }

    # The existing installer gates on exact private device backup,
    # factory partition, firmware checksum, and ESP32-P4 chip revision.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer `
        -Port $Port `
        -Firmware $candidate `
        -BackupFolder $BackupFolder `
        -ConfirmFlash 'FLASH CAMERA'
    if ($LASTEXITCODE -ne 0) { throw 'Safe application-only camera flash failed' }
    Write-Host 'ONBOARD CAMERA ROTATION FIX FLASHED SUCCESSFULLY' -ForegroundColor Green
}
catch {
    Write-Warning "STOP: $($_.Exception.Message)"
    exit 1
}
