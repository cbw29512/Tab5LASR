# Windows PowerShell 5.1. Download exactly one successful, review-only
# GitHub SD diagnostic, then run the current-camera safety preflight.
# NO FLASH by default; -Flash also requires a typed confirmation.
param(
    [Parameter(Mandatory=$true)][long]$RunId,
    [Parameter(Mandatory=$true)][string]$CameraBackupFolder,
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [switch]$Flash
)
$ErrorActionPreference = 'Stop'
$repo = 'cbw29512/Tab5LASR'
$branch = 'feat/lasr-microsd-session-core-20261118'
try {
    # Fail before writing or flashing if the review run is not successful.
    $runText = & gh api "repos/$repo/actions/runs/$RunId"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect GitHub CI run' }
    $run = ($runText -join "`n") | ConvertFrom-Json
    if ($run.status -cne 'completed' -or $run.conclusion -cne 'success' -or
        $run.head_branch -cne $branch) {
        throw 'Selected CI run must be successful and from the LASR review branch'
    }
    # Query the real artifact listing; PR runs may use a synthetic merge SHA
    # in the artifact name, so never invent an artifact filename from the head.
    $artifactText = & gh api "repos/$repo/actions/runs/$RunId/artifacts?per_page=100"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect GitHub diagnostic artifacts' }
    $artifactResponse = ($artifactText -join "`n") | ConvertFrom-Json
    $matches = @($artifactResponse.artifacts | Where-Object {
        $_.name -like 'lasr-optin-sd-smoke-*' -and $_.expired -eq $false
    })
    if ($matches.Count -ne 1) {
        throw "Expected exactly one unexpired SD diagnostic artifact; found $($matches.Count)"
    }
    $folder = Join-Path $HOME ("Downloads\Tab5LASR_SD_" + $RunId)
    if (Test-Path -LiteralPath $folder) {
        throw "Destination exists; inspect/remove it manually before retrying: $folder"
    }
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    & gh run download $RunId --repo $repo --name $matches[0].name --dir $folder
    if ($LASTEXITCODE -ne 0) { throw 'GitHub artifact download failed' }
    $preflight = Join-Path $PSScriptRoot 'PREFLIGHT_LASR_DIAGNOSTIC.ps1'
    if (!(Test-Path -LiteralPath $preflight -PathType Leaf)) {
        throw 'The current-camera recovery preflight script is missing'
    }
    Write-Host "Review run head: $($run.head_sha)" -ForegroundColor Cyan
    Write-Host "Downloaded artifact to $folder" -ForegroundColor Cyan
    Write-Host 'Selected SD-only diagnostic; this firmware does NOT run the camera.' -ForegroundColor Yellow
    # Delegate every hash, partition, live-flash and typed-consent check.
    # PowerShell switch splatting does not bypass any preflight prompt.
    & $preflight -CandidateFolder $folder -CameraBackupFolder $CameraBackupFolder `
        -Port $Port -Mode 'sd-smoke' -Flash:$Flash
    if ($LASTEXITCODE -ne 0) { throw 'Device recovery preflight did not pass' }
    Write-Host 'LASR SD diagnostic preparation completed.' -ForegroundColor Green
}
catch {
    Write-Warning "SD DIAGNOSTIC STOPPED: $($_.Exception.Message)"
    exit 1
}