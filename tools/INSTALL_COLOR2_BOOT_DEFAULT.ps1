# Install reviewed Color 2 boot default to Tab5's app partition only.
param(
 [ValidatePattern('^COM[0-9]+$')][string]$Port='COM5',
 [string]$OriginalBackupFolder="$HOME\Tab5Recovery\20261117-173241",
 [string]$CameraBackupFolder="$HOME\Tab5Recovery\camera-working-20261117-212814",
 [string]$ConfirmFlash=''
)
$ErrorActionPreference='Stop'
try {
 $repo='cbw29512/Tab5LASR'
 $commit='5cff8a61bd48a698dfe529b7817af2b46d8a8919'
 $runId=44252445365
 # Both private recovery images must exist and match their verified hashes.
 $current=Join-Path $CameraBackupFolder 'tab5-lasr-camera-full-verified.bin'
 $marker=Join-Path $CameraBackupFolder 'PRIVATE_RELEASE_VERIFIED.txt'
 $manifestPath=Join-Path $CameraBackupFolder 'SHA256-MANIFEST.json'
 foreach($item in @($current,$marker,$manifestPath)){
   if(!(Test-Path -LiteralPath $item -PathType Leaf)){throw "Missing camera recovery file: $item"}
 }
 $expected='C1695018BFE1B1871F2C468C606A47F1A18311BAC5C2D934DFBE1258E52F2773'
 $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
 $hash=(Get-FileHash -LiteralPath $current -Algorithm SHA256).Hash.ToUpperInvariant()
 if((Get-Item -LiteralPath $current).Length -ne 16777216 -or $hash -cne $expected -or $manifest.sha256 -cne $expected){
   throw 'Current working-camera backup failed integrity checks'
 }
 # Reject build errors or an artifact from another source commit.
 $run=& gh run view $runId --repo $repo --json status,conclusion,headSha | ConvertFrom-Json
 if($LASTEXITCODE -ne 0 -or !$run){throw 'Cannot query GitHub build'}
 if($run.status -ne 'completed' -or $run.conclusion -ne 'success' -or $run.headSha -cne $commit){
   throw 'Color 2 firmware CI is not green at the pinned revision'
 }
 $folder=Join-Path (Split-Path -Parent $PSScriptRoot) 'onboard-color2-5cff8a61b'
 if(Test-Path -LiteralPath $folder){throw "Refuse to reuse old artifact directory: $folder"}
 New-Item -ItemType Directory -Path $folder | Out-Null
 & gh run download $runId --repo $repo --name "tab5lasr-onboard-$commit" --dir $folder
 if($LASTEXITCODE -ne 0){throw 'Download of verified firmware failed'}
 $image=Join-Path $folder 'tab5lasr_onboard.bin'
 if(!(Test-Path -LiteralPath $image -PathType Leaf)){throw 'Downloaded image is missing'}
 # The existing guarded flasher checks original dice backup, manifest,
 # original partition layout, SHA-256, size and ESP32-P4 rev1.3 metadata.
 $installer=Join-Path $PSScriptRoot 'FLASH_CAMERA_ONLY.ps1'
 & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -Port $Port -Firmware $image -BackupFolder $OriginalBackupFolder -ConfirmFlash $ConfirmFlash
 if($LASTEXITCODE -ne 0){throw 'App-only update failed or was canceled'}
 Write-Host 'COLOR 2 STARTUP DEFAULT INSTALLED. CHECK CAMERA AND BATTERY.' -ForegroundColor Green
}
catch{
 Write-Warning "UPDATE STOPPED: $($_.Exception.Message)"
 exit 1
}
