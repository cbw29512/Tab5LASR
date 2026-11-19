# Windows PowerShell 5.1. Read-only unless -Flash + typed FLASH consent.
# Requires exact successful combined CI and an exact installed diagnostic match.
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
        throw 'Combined CI must be successful at exactly the selected source SHA'
    }
    $artifactText = & gh api "repos/$repo/actions/runs/$RunId/artifacts?per_page=100"
    if ($LASTEXITCODE -ne 0) { throw 'GitHub artifacts lookup failed' }
    $list = ($artifactText -join "`n") | ConvertFrom-Json
    $matches = @($list.artifacts | Where-Object {
        $_.name -like 'lasr-optin-combined-*' -and $_.expired -eq $false
    })
    if ($matches.Count -ne 1) {
        throw "Expected one nonexpired combined artifact, found $($matches.Count)"
    }
    $folder = Join-Path $HOME ("Downloads\Tab5LASR_COMBINED_" + $RunId)
    if (Test-Path -LiteralPath $folder) {
        throw "Candidate already downloaded; inspect rather than overwrite: $folder"
    }
    New-Item -ItemType Directory -Path $folder -ErrorAction Stop | Out-Null
    & gh run download $RunId --repo $repo --name $matches[0].name --dir $folder
    if ($LASTEXITCODE -ne 0) { throw 'Exact combined artifact download failed' }
    $preflight = Join-Path $PSScriptRoot 'PREFLIGHT_LASR_DIAGNOSTIC.ps1'
    if (!(Test-Path -LiteralPath $preflight -PathType Leaf)) {
        throw 'Verified recovery preflight script missing'
    }
    Write-Host "Combined artifact from verified run $RunId at $ExpectedHeadSha" -ForegroundColor Cyan
    Write-Host "Read-only default; -Flash still requires typing FLASH" -ForegroundColor Yellow
    & $preflight -CandidateFolder $folder -CameraBackupFolder $CameraBackupFolder `
        -InstalledDiagnosticFolder $InstalledDiagnosticFolder -Port $Port `
        -Mode 'combined' -Flash:$Flash
    if ($LASTEXITCODE -ne 0) { throw 'Device preflight or install failed' }
    Write-Host 'COMBINED REVIEW PROCEDURE COMPLETED' -ForegroundColor Green
} catch {
    Write-Warning "COMBINED TEST BLOCKED: $($_.Exception.Message)"
    exit 1
}
