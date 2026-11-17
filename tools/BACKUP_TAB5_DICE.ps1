# Read-only recovery acquisition for ESP32-P4 Tab5; PowerShell 5.1 compatible.
# No flash/erase/write commands appear in this script.
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [string]$PrivateRepo = '',
    [string]$Root = "$HOME\Tab5Recovery"
)
$ErrorActionPreference = 'Stop'
try {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $folder = Join-Path $Root $timestamp
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    $first = Join-Path $folder 'tab5-flash-read1.bin'
    $second = Join-Path $folder 'tab5-flash-read2.bin'
    $verified = Join-Path $folder 'tab5-dice-full-verified.bin'
    $manifest = Join-Path $folder 'SHA256-MANIFEST.json'

    # The installed Tab5 has a physically verified 16 MiB SPI flash.
    # Read twice to catch transient USB/read errors; do not alter bootloader or partitions.
    foreach ($destination in @($first, $second)) {
        Write-Host "READ-ONLY: copying the complete 16 MiB device to $destination"
        & python -m esptool --chip esp32p4 --port $Port --baud 460800 read-flash 0x0 0x1000000 $destination
        if ($LASTEXITCODE -ne 0) { throw "esptool read failed: $destination" }
        if ((Get-Item -LiteralPath $destination).Length -ne 16777216) {
            throw "Unexpected backup length: $destination"
        }
    }

    $h1 = (Get-FileHash -Algorithm SHA256 -LiteralPath $first).Hash.ToUpperInvariant()
    $h2 = (Get-FileHash -Algorithm SHA256 -LiteralPath $second).Hash.ToUpperInvariant()
    if ($h1 -cne $h2) { throw "Two complete flash reads have DIFFERENT SHA-256 hashes" }
    Copy-Item -LiteralPath $first -Destination $verified -ErrorAction Stop
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $verified).Hash.ToUpperInvariant()
    if ($hash -cne $h1) { throw 'Copy verification failed' }

    $data = [ordered]@{
        board = 'M5Stack Tab5'
        chip = 'ESP32-P4 rev v1.3 (previously verified; recheck connected device)'
        acquired_at_local = (Get-Date).ToString('o')
        port = $Port
        range_start = '0x000000'
        length_bytes = 16777216
        filename = 'tab5-dice-full-verified.bin'
        sha256 = $hash
        source_read_hashes = @($h1, $h2)
        application_only_flash_offset = '0x10000 (subject to partition verification)'
        warning = 'PRIVATE full-device flash image; do not upload to public Tab5LASR.'
    }
    $data | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifest -Encoding UTF8
    Write-Host "LOCAL VERIFIED: $verified" -ForegroundColor Green
    Write-Host "SHA-256: $hash" -ForegroundColor Green

    if ([string]::IsNullOrWhiteSpace($PrivateRepo)) {
        Write-Warning 'Only a LOCAL backup exists. Supply -PrivateRepo OWNER/PRIVATE_REPO to archive and verify permanently.'
        return
    }
    $repoInfo = & gh repo view $PrivateRepo --json visibility | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or !$repoInfo -or $repoInfo.visibility -ne 'PRIVATE') {
        throw "Refusing upload: $PrivateRepo must be an existing PRIVATE GitHub repository."
    }
    $tag = "tab5-dice-readback-$timestamp"
    & gh release create $tag $verified $manifest --repo $PrivateRepo --title "Verified Tab5 dice roller flash $timestamp" --notes "Full 16 MiB physical-device readback. SHA-256 in manifest. Restore is manual; never flash this automatically."
    if ($LASTEXITCODE -ne 0) { throw "Private GitHub Release upload failed" }

    # Download again from GitHub; verify the bytes, not merely the upload result.
    $downloadDir = Join-Path $folder 'download-verify'
    New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
    & gh release download $tag --repo $PrivateRepo --pattern 'tab5-dice-full-verified.bin' --dir $downloadDir
    if ($LASTEXITCODE -ne 0) { throw 'Private release download failed' }
    $remote = Join-Path $downloadDir 'tab5-dice-full-verified.bin'
    if ((Get-Item -LiteralPath $remote).Length -ne 16777216) { throw 'Downloaded backup size mismatch' }
    $remoteHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $remote).Hash.ToUpperInvariant()
    if ($remoteHash -cne $hash) { throw 'Downloaded backup SHA-256 mismatch' }

    $verifiedMarker = Join-Path $folder 'PRIVATE_RELEASE_VERIFIED.txt'
    "PrivateRepo=$PrivateRepo`nReleaseTag=$tag`nSHA256=$hash`nVerifiedAt=$((Get-Date).ToString('o'))" | Set-Content -LiteralPath $verifiedMarker -Encoding UTF8
    Write-Host "PERMANENT PRIVATE RELEASE VERIFIED: $PrivateRepo / $tag" -ForegroundColor Green
    Write-Host 'SAFE GATE PASSED: exact installed flash backup retained locally and in private GitHub.' -ForegroundColor Green
}
catch {
    Write-Warning "BACKUP NOT VERIFIED: $($_.Exception.Message)"
    exit 1
}
