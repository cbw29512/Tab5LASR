# READ ONLY: identify exact installed Tab5 LASR firmware using known review artifacts.
# No flash writes, no erases, no changes to the private camera backup.
# Data schema: 16 MiB live/recovery dumps and signed candidate app + hash + policy.
# State flow: VERIFY BACKUP -> READ FLASH -> COMPARE KNOWN BUILDS -> REPORT.
param(
    [Parameter(Mandatory=$true)][string]$CameraBackupFolder,
    [Parameter(Mandatory=$true)][string[]]$PreviousArtifactFolders,
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5'
)
$ErrorActionPreference = 'Stop'
$live = $null
try {
    $backup = (Resolve-Path -LiteralPath $CameraBackupFolder).Path
    $original = Join-Path $backup 'tab5-lasr-camera-full-verified.bin'
    $manifestFile = Join-Path $backup 'SHA256-MANIFEST.json'
    $markerFile = Join-Path $backup 'PRIVATE_RELEASE_VERIFIED.txt'
    foreach ($file in @($original, $manifestFile, $markerFile)) {
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Missing private recovery evidence: $file"
        }
    }
    $manifest = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $original).Hash.ToUpperInvariant()
    $marker = Get-Content -LiteralPath $markerFile -Raw
    if ((Get-Item -LiteralPath $original).Length -ne 16777216 -or
        $manifest.filename -cne 'tab5-lasr-camera-full-verified.bin' -or
        $hash -cne $manifest.sha256.ToUpperInvariant() -or
        $marker -notmatch [regex]::Escape($hash) -or
        !$manifest.source_read_hashes -or $manifest.source_read_hashes.Count -ne 2 -or
        $manifest.source_read_hashes[0] -cne $hash -or
        $manifest.source_read_hashes[1] -cne $hash) {
        throw 'Private camera recovery backup is not fully verified'
    }

    # Reuse the original exact-byte guard; do not invent a permissive matcher.
    $guard = Join-Path $PSScriptRoot 'verify_installed_sd_upgrade.py'
    if (!(Test-Path -LiteralPath $guard -PathType Leaf)) {
        throw 'Exact flash comparison guard unavailable'
    }
    $live = Join-Path ([IO.Path]::GetTempPath()) (
        "lasr-identify-" + [guid]::NewGuid().ToString('N') + '.bin')
    Write-Host 'READ ONLY: reading flash for exact identification.' -ForegroundColor Cyan
    & python -m esptool --chip esp32p4 --port $Port --baud 460800 `
        read-flash 0x0 0x1000000 $live
    if ($LASTEXITCODE -ne 0 -or
        !(Test-Path -LiteralPath $live -PathType Leaf) -or
        (Get-Item -LiteralPath $live).Length -ne 16777216) {
        throw 'Connected Tab5 flash read did not complete'
    }

    $verifiedInstalledFolders = @()
    foreach ($candidate in $PreviousArtifactFolders) {
        if (!(Test-Path -LiteralPath $candidate -PathType Container)) {
            Write-Host "SKIPPED absent artifact folder: $candidate" -ForegroundColor Yellow
            continue
        }
        $folder = (Resolve-Path -LiteralPath $candidate -ErrorAction Stop).Path
        $app = Join-Path $folder 'tab5lasr_onboard.bin'
        $sums = Join-Path $folder 'SHA256SUMS.txt'
        $policyFile = Join-Path $folder 'FLASH_POLICY.txt'
        foreach ($file in @($app,$sums,$policyFile)) {
            if (!(Test-Path -LiteralPath $file -PathType Leaf)) {
                throw "Missing previous build evidence: $file"
            }
        }
        $expected = (((Get-Content -LiteralPath $sums |
            Select-Object -First 1) -split '\s+')[0]).ToUpperInvariant()
        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $app).Hash.ToUpperInvariant()
        $policy = Get-Content -LiteralPath $policyFile -Raw
        if ($expected.Length -ne 64 -or $actual -cne $expected -or
            $policy -notmatch '(?m)^MODE=(sd-smoke|combined)\r?$' -or
            $policy -notmatch '(?m)^APP_OFFSET=0x10000\r?$' -or
            $policy -notmatch '(?m)^MAX_PARTITION=1048576\r?$' -or
            $policy -notmatch '(?m)^FLASH=NO_AUTO_FLASH\r?$') {
            throw "Candidate hash or flash policy rejected: $folder"
        }
        Write-Host "Checking exact installed image: $folder"
        & python $guard $original $live $app
        if ($LASTEXITCODE -eq 0) {
            $verifiedInstalledFolders += $folder
            Write-Host "EXACT MATCH: $folder" -ForegroundColor Green
        } else {
            Write-Host "NOT MATCHED: $folder" -ForegroundColor Yellow
        }
    }
    if ($verifiedInstalledFolders.Count -eq 0) {
        throw 'No known artifact exactly matches the installed firmware. DO NOT FLASH.'
    }
    Write-Host 'EXACT INSTALLED REFERENCE(S), READ ONLY:' -ForegroundColor Green
    $verifiedInstalledFolders | ForEach-Object { Write-Host $_ -ForegroundColor Green }
    Write-Host 'No flash writes performed. Use only an exact match in the original guarded preflight.' -ForegroundColor Green
}
catch {
    Write-Warning "IDENTIFICATION STOPPED: $($_.Exception.Message)"
    exit 1
}
finally {
    if ($live -and (Test-Path -LiteralPath $live -PathType Leaf)) {
        Remove-Item -LiteralPath $live -Force -ErrorAction SilentlyContinue
    }
}
