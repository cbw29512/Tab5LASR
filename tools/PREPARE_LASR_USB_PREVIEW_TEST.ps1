# Windows PowerShell 5.1. Read-only unless -Flash + typed FLASH consent.
# Requires exact successful USB preview CI and matching previously installed app.
param(
    [Parameter(Mandatory=$true)][long]$RunId,
    [Parameter(Mandatory=$true)][string]$ExpectedHeadSha,
    [Parameter(Mandatory=$true)][string]$CameraBackupFolder,
    [Parameter(Mandatory=$true)][string]$InstalledDiagnosticFolder,
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [switch]$Flash
)
$ErrorActionPreference='Stop'
$repo='cbw29512/Tab5LASR'
$branch='feat/lasr-camera-sd-concurrent-hardware-gate-20261119'
try {
    if ($ExpectedHeadSha -cnotmatch '^[a-f0-9]{40}$') {
        throw 'ExpectedHeadSha must be the complete verified GitHub commit SHA'
    }
    # Verify the CI run before downloading or inspecting any device flash.
    $runText = & gh api "repos/$repo/actions/runs/$RunId"
    if ($LASTEXITCODE -ne 0) { throw 'GitHub CI run lookup failed' }
    $run = ($runText -join "`n") | ConvertFrom-Json
    if ($run.status -cne 'completed' -or $run.conclusion -cne 'success' -or
        $run.head_branch -cne $branch -or $run.head_sha -cne $ExpectedHeadSha) {
        throw 'USB preview CI must succeed at the exact selected source SHA'
    }
    $artifactText = & gh api "repos/$repo/actions/runs/$RunId/artifacts?per_page=100"
    if ($LASTEXITCODE -ne 0) { throw 'GitHub artifacts lookup failed' }
    $list = ($artifactText -join "`n") | ConvertFrom-Json
    $matches = @($list.artifacts | Where-Object {
        $_.name -like 'lasr-optin-usb-preview-*' -and $_.expired -eq $false
    })
    if ($matches.Count -ne 1) {
        throw "Expected one nonexpired USB preview artifact, found $($matches.Count)"
    }
    $folder = Join-Path $HOME ("Downloads\Tab5LASR_USB_PREVIEW_" + $RunId)
    if (Test-Path -LiteralPath $folder -PathType Container) {
        # The user can safely retry; the preflight below rechecks the
        # candidate SHA-256, mode and partition instead of blindly trusting it.
        Write-Host "Reusing existing candidate folder: $folder" -ForegroundColor Cyan
    } else {
        if (Test-Path -LiteralPath $folder) {
            throw 'Candidate destination exists but is not a directory'
        }
        New-Item -ItemType Directory -Path $folder -ErrorAction Stop | Out-Null
        & gh run download $RunId --repo $repo --name $matches[0].name --dir $folder
        if ($LASTEXITCODE -ne 0) { throw 'Exact USB preview artifact download failed' }
    }
    $preflight = Join-Path $PSScriptRoot 'PREFLIGHT_LASR_DIAGNOSTIC.ps1'
    if (!(Test-Path -LiteralPath $preflight -PathType Leaf)) {
        throw 'Verified recovery preflight script missing'
    }
    Write-Host "USB preview artifact from verified run $RunId at $ExpectedHeadSha" -ForegroundColor Cyan
    Write-Host "Read-only default; -Flash still requires typing FLASH" -ForegroundColor Yellow
    & $preflight -CandidateFolder $folder -CameraBackupFolder $CameraBackupFolder `
        -InstalledDiagnosticFolder $InstalledDiagnosticFolder -Port $Port `
        -Mode 'usb-preview' -Flash:$Flash
    if ($LASTEXITCODE -ne 0) { throw 'Device preflight or install failed' }
    Write-Host 'USB PREVIEW REVIEW PROCEDURE COMPLETED' -ForegroundColor Green
} catch {
    Write-Warning "USB PREVIEW TEST BLOCKED: $($_.Exception.Message)"
    exit 1
}
