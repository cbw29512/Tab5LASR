# Application-only installer. NEVER call before private recovery verification.
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [Parameter(Mandatory=$true)][string]$Firmware,
    [Parameter(Mandatory=$true)][string]$BackupFolder,
    # Explicit confirmation avoids a blank Read-Host in nested PowerShell.
    [string]$ConfirmFlash = ''
)
$ErrorActionPreference = 'Stop'
try {
    $Firmware = (Resolve-Path -LiteralPath $Firmware).Path
    $BackupFolder = (Resolve-Path -LiteralPath $BackupFolder).Path
    $recovery = Join-Path $BackupFolder 'tab5-dice-full-verified.bin'
    $manifestPath = Join-Path $BackupFolder 'SHA256-MANIFEST.json'
    $marker = Join-Path $BackupFolder 'PRIVATE_RELEASE_VERIFIED.txt'
    if (!(Test-Path -LiteralPath $marker) -or
        !(Test-Path -LiteralPath $manifestPath) -or
        !(Test-Path -LiteralPath $recovery)) {
        throw 'Private permanent backup marker, image, or manifest missing. Do not flash.'
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ((Get-Item -LiteralPath $recovery).Length -ne 16777216) { throw 'Backup size is wrong' }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $recovery).Hash.ToUpperInvariant()
    if ($actual -cne $manifest.sha256) { throw 'Recovery SHA-256 mismatch' }

    # Check the original device partition table rather than assume flash offsets.
    $gate = Join-Path $PSScriptRoot 'verify_factory_partition.py'
    & python $gate $recovery $Firmware
    if ($LASTEXITCODE -ne 0) { throw 'Original device partition verification failed' }

    # The review package includes the build-produced hash.
    $sums = Join-Path (Split-Path -Parent $Firmware) 'SHA256SUMS.txt'
    if (!(Test-Path -LiteralPath $sums)) { throw 'Camera firmware SHA256SUMS.txt missing' }
    $expected = ((Get-Content -LiteralPath $sums | Select-Object -First 1) -split '\s+')[0].ToUpperInvariant()
    $imageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Firmware).Hash.ToUpperInvariant()
    if ($imageHash -cne $expected) { throw 'Camera firmware SHA-256 mismatch' }
    $info = & python -m esptool --chip esp32p4 image-info $Firmware
    if ($LASTEXITCODE -ne 0) { throw 'Camera firmware not a valid ESP32-P4 image' }
    $infoText = $info -join "`n"
    if ($infoText -notmatch 'Minimal chip revision: v1\.0' -or
        $infoText -notmatch 'Maximal chip revision: v1\.99') {
        throw 'Camera image does not declare compatibility with ESP32-P4 v1.3'
    }
    Write-Host 'Recovery flash and candidate application verified.' -ForegroundColor Green
    Write-Host 'Camera test will REPLACE the installed dice-roller APPLICATION.' -ForegroundColor Yellow
    # A deliberate CLI token is accepted only AFTER all safety gates passed.
    # Leave the token unset for an interactive prompt in the same shell.
    $confirm = $ConfirmFlash
    if ([string]::IsNullOrWhiteSpace($confirm)) {
        $confirm = Read-Host 'Type FLASH CAMERA to install application at 0x10000'
    }
    if ($confirm -cne 'FLASH CAMERA') {
        throw 'Cancelled; no flash was written.'
    }
    Write-Host 'Operator confirmed application-only flash at 0x10000.' -ForegroundColor Yellow
    & python -m esptool --chip esp32p4 --port $Port --baud 460800 write-flash 0x10000 $Firmware
    if ($LASTEXITCODE -ne 0) { throw 'Camera application flash failed' }
    Write-Host 'CAMERA APPLICATION FLASH COMPLETED. Physically test the selected camera, battery and logs.' -ForegroundColor Green
} catch {
    Write-Warning "INSTALL STOPPED: $($_.Exception.Message)"
    exit 1
}
