# Fix for PowerShell 5.1 "Could not resolve to a Repository" failure

The earlier bootstrap called `gh repo view cbw29512/Tab5DiceRecovery ... 2>$null` while `$ErrorActionPreference="Stop"` was enabled. In Windows PowerShell 5.1, the native CLI's error output can throw before its `$LASTEXITCODE` check. **It stopped before any flash read or write**.

The private recovery repository must be created first (once), before running the read-only backup. GitHub CLI supports `--private --add-readme`:

```powershell
$ErrorActionPreference = "Stop"
try {
    # We verified the repo was not accessible when the first bootstrap failed.
    & gh repo create cbw29512/Tab5DiceRecovery --private --add-readme
    if ($LASTEXITCODE -ne 0) { throw "Private repository creation failed" }

    $visibility = & gh repo view cbw29512/Tab5DiceRecovery --json visibility --jq .visibility
    if ($LASTEXITCODE -ne 0 -or $visibility -ne "PRIVATE") {
        throw "Repo did not verify as PRIVATE"
    }

    $project = Join-Path $HOME "Tab5LASR"
    if (Test-Path (Join-Path $project ".git")) {
        & git -C $project pull --ff-only
    } else {
        & gh repo clone cbw29512/Tab5LASR $project
    }
    if ($LASTEXITCODE -ne 0) { throw "GitHub project retrieval failed" }

    & powershell -NoProfile -ExecutionPolicy Bypass -File `
        (Join-Path $project "tools\BACKUP_TAB5_DICE.ps1") `
        -Port COM5 -PrivateRepo cbw29512/Tab5DiceRecovery
    if ($LASTEXITCODE -ne 0) { throw "Device backup/readback validation failed" }
    Write-Host "PERMANENT PRIVATE BACKUP VERIFIED" -ForegroundColor Green
}
catch {
    Write-Warning "STOP: $($_.Exception.Message)"
}
```

If `gh repo create` reports it already exists, **stop and inspect** using `gh repo view cbw29512/Tab5DiceRecovery --json visibility`; never upload a flash dump to an unverified public repository.

`COM5` was the historical port for the user's ESP32-P4; check Device Manager before running. The backup script is read-only. No Tab5 flashing takes place in this sequence.
