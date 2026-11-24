# Windows PowerShell 5.1 - READ-ONLY inspection of a removed microSD card.
# Verifies the fixed, explicitly synthetic session fixture, never real scores.
param([Parameter(Mandatory=$true)][string]$CardRoot)
$ErrorActionPreference = 'Stop'
try {
    # A path to the mounted card (example E:\), NOT the Tab5's COM port.
    $root = (Resolve-Path -LiteralPath $CardRoot -ErrorAction Stop).Path
    $base = Join-Path $root 'LASR\diagnostics'
    if (!(Test-Path -LiteralPath $base -PathType Container)) {
        throw "LASR diagnostic directory not found on card: $base"
    }
    $folders = @(Get-ChildItem -LiteralPath $base -Directory -Filter 'synthetic-session-*')
    if ($folders.Count -eq 0) { throw 'No synthetic diagnostic sessions on microSD' }

    # Exact, known fixture from lasr_sd_session_probe.c; no assumed timing.
    $expectedRows = @(
        'shot,elapsed_ms,split_ms,zone,score,x_norm,y_norm',
        '1,150,150,UNSCORED,0,0.250000,0.500000',
        '2,410,260,UNSCORED,0,0.750000,0.400000'
    )
    $expectedCsv = ($expectedRows -join "`n") + "`n"
    foreach ($dir in $folders) {
        # Validate ID so a misleading directory cannot redirect inspection.
        $id = $dir.Name.Substring('synthetic-session-'.Length)
        if ($id -cnotmatch '^[a-zA-Z0-9_-]{1,40}$') {
            throw "Invalid diagnostic session directory: $($dir.Name)"
        }
        $session = Join-Path $dir.FullName 'LASR\sessions\fake_shots'
        $csv = Join-Path $session 'shots.csv'
        $journal = Join-Path $session 'shots.pending'
        $metadata = Join-Path $session 'session.json'
        $targetFile = Join-Path $session 'target.json'
        foreach ($file in @($csv, $journal, $metadata, $targetFile)) {
            if (!(Test-Path -LiteralPath $file -PathType Leaf)) {
                throw "Missing required session file: $file"
            }
        }
        $csvText = [IO.File]::ReadAllText($csv)
        $journalText = [IO.File]::ReadAllText($journal)
        if ($csvText -cne $expectedCsv -or $journalText -cne $expectedCsv) {
            throw "CSV/journal mismatch or corrupted synthetic shots: $session"
        }
        $meta = Get-Content -LiteralPath $metadata -Raw | ConvertFrom-Json
        $target = Get-Content -LiteralPath $targetFile -Raw | ConvertFrom-Json
        if ($meta.schema_version -ne 1 -or $meta.session_id -cne 'fake_shots' -or
            $meta.target_id -cne 'synthetic_test' -or
            $meta.status -cne 'complete' -or $meta.save_state -cne 'saved' -or
            $meta.shot_count -ne 2 -or $meta.score_total -ne 0 -or
            $meta.elapsed_ms -ne 410 -or $meta.start_delay_ms -ne 3000 -or
            $target.schema_version -ne 1 -or
            $target.target_id -cne 'synthetic_test' -or
            $target.calibration_revision -ne 1) {
            throw "Synthetic session metadata invalid: $session"
        }
        Write-Host "PASS: $($dir.Name) - 2 synthetic events, exact journal/CSV, JSON metadata" -ForegroundColor Green
    }
    Write-Host "PASS: $($folders.Count) diagnostic session(s) verified on microSD. NO FILES MODIFIED." -ForegroundColor Green
} catch {
    Write-Warning "LASR SD CARD VERIFICATION FAILED: $($_.Exception.Message)"
    exit 1
}