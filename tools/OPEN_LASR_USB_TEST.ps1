# Windows PowerShell 5.1: host the real USB diagnostic UI on localhost only.
# Never flashes the Tab5 and never opens a network listener off the PC.
param([ValidateRange(1024,65535)][int]$Port = 8765)
$ErrorActionPreference = 'Stop'
$server = $null
try {
    $web = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\web')).Path
    $page = Join-Path $web 'serial-test.html'
    if (!(Test-Path -LiteralPath $page -PathType Leaf)) {
        throw 'USB laser diagnostic webpage is missing'
    }
    $python = Get-Command python -ErrorAction Stop
    if ($python.CommandType -ne 'Application') {
        throw 'Python executable not found (needed for local HTTP file serving)'
    }
    # The browser requires a trustworthy localhost origin for Web Serial.
    # Bind only loopback; no network exposure or camera stream is created.
    $server = Start-Process -FilePath $python.Source -WorkingDirectory $web `
        -ArgumentList @('-m','http.server',"$Port",'--bind','127.0.0.1') `
        -PassThru
    Start-Sleep -Milliseconds 1200
    $server.Refresh()
    if ($server.HasExited) {
        throw 'Local web server exited. Port may already be in use.'
    }
    $url = "http://127.0.0.1:$Port/serial-test.html"
    $pageResult = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 4
    if ($pageResult.StatusCode -ne 200 -or
        $pageResult.Content -notmatch 'Tab5 LASR.*USB Laser Test') {
        throw 'Local web server did not serve the expected USB test page'
    }
    Write-Host "LOCAL LASR USB TEST: $url" -ForegroundColor Green
    Write-Host 'Select Connect Tab5 USB in desktop Chrome or Edge.' -ForegroundColor Cyan
    Write-Host 'No live camera stream; this page reads real serial candidates only.' -ForegroundColor Yellow
    Start-Process $url
}
catch {
    if ($server) {
        try { if (!$server.HasExited) { $server.Kill() } }
        catch { Write-Warning "Could not stop local server: $($_.Exception.Message)" }
    }
    Write-Warning "USB WEB TEST DID NOT START: $($_.Exception.Message)"
    exit 1
}
