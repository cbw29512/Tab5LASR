# RGB565 color-test app-only installer, Windows PowerShell 5.1
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [string]$BackupFolder = "$HOME\Tab5Recovery\20261117-173241"
)
$ErrorActionPreference = 'Stop'
try {
    $repository = "cbw29512/Tab5LASR"
    $runId = 44208623788
    $commit = "d2946decd4aa8080612b69271b6998c34478f0de"
    $artifact = "tab5lasr-onboard-$commit"
    $project = Split-Path -Parent $PSScriptRoot
    $folder = Join-Path $project "onboard-color-test"
    # The exact CI run must verify the tested commit before any write.
    $run = & gh run view $runId --repo $repository --json status,conclusion,headSha | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or !$run) { throw "Cannot read GitHub verification" }
    if ($run.status -ne 'completed' -or $run.conclusion -ne 'success' -or $run.headSha -ne $commit) {
        throw "Build is not successful for the expected code; no flash performed."
    }
    if (Test-Path -LiteralPath $folder) { throw "Download directory already exists: $folder" }
    New-Item -ItemType Directory -Path $folder | Out-Null
    & gh run download $runId --repo $repository --name $artifact --dir $folder
    if ($LASTEXITCODE -ne 0) { throw "GitHub artifact download failed" }
    $binary = Join-Path $folder 'tab5lasr_onboard.bin'
    # Reuses permanent private backup, partition, checksum and revision gates.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'FLASH_CAMERA_ONLY.ps1') -Port $Port -Firmware $binary -BackupFolder $BackupFolder -ConfirmFlash 'FLASH CAMERA'
    if ($LASTEXITCODE -ne 0) { throw "Safe application-only install failed" }
    Write-Host 'ONBOARD CAMERA COLOR TEST INSTALLED' -ForegroundColor Green
}
catch {
    Write-Warning "STOP: $($_.Exception.Message)"
    exit 1
}
