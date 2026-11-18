# Physical device read-only check of proposed web camera partition expansion.
# Compatible with Windows PowerShell 5.1 and esptool 5.6.0.
# Commands inside this file NEVER erase or write flash.
param(
    [ValidatePattern('^COM[0-9]+$')][string]$Port = 'COM5',
    [string]$Project = "$HOME\Tab5LASR",
    [string]$Backup = "$HOME\Tab5Recovery\20261117-173241\tab5-dice-full-verified.bin"
)
$ErrorActionPreference = 'Stop'

try {
    if (!(Test-Path -LiteralPath $Backup -PathType Leaf)) {
        throw 'Original verified 16 MiB dice-roller readback not found'
    }
    $expected = '52545123E6C20C4B139F290B64E4C70701C3618616D9A4149D444039CE7A3C01'
    $actual = (Get-FileHash -LiteralPath $Backup -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($actual -cne $expected) {
        throw 'Local backup does not match permanent verified private release'
    }
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $folder = Join-Path $HOME "Tab5Recovery\web-expansion-readonly-$stamp"
    New-Item -Path $folder -ItemType Directory -Force | Out-Null
    $read1 = Join-Path $folder 'region-read-1.bin'
    $read2 = Join-Path $folder 'region-read-2.bin'
    foreach ($path in @($read1, $read2)) {
        Write-Host "READ-ONLY 0x110000..0x210000 to $path" -ForegroundColor Cyan
        & python -m esptool --chip esp32p4 --port $Port --baud 460800 read-flash 0x110000 0x100000 $path
        if ($LASTEXITCODE -ne 0) {
            throw "Physical flash read failed: $path"
        }
        if ((Get-Item -LiteralPath $path).Length -ne 1048576) {
            throw "Incomplete region read: $path"
        }
    }
    $hash1 = (Get-FileHash -LiteralPath $read1 -Algorithm SHA256).Hash
    $hash2 = (Get-FileHash -LiteralPath $read2 -Algorithm SHA256).Hash
    if ($hash1 -cne $hash2) {
        throw 'Two independent flash region reads do not match'
    }
    $inspector = Join-Path $Project 'tools\audit_factory_expansion.py'
    & python $inspector $Backup --read1 $read1 --read2 $read2
    if ($LASTEXITCODE -ne 0) {
        throw 'Expansion region contains data or cannot be verified'
    }
    Write-Host 'READ-ONLY REGION CHECK PASSED; NO FLASH CHANGED' -ForegroundColor Green
    Write-Warning 'This is not permission to change the partition table or update the ESP32-C6.'
    Write-Host "Original local region sample retained: $folder"
}
catch {
    Write-Warning "EXPANSION REVIEW STOPPED: $($_.Exception.Message)"
    exit 1
}
