# RETIRED. Physical camera testing established Color 2 as closer to normal.
# Do NOT install the older Color 1 build by accident.
$ErrorActionPreference = 'Stop'
try {
    throw 'Color 1 installer retired. Use the new Color 2 release installer only after its exact firmware CI build passes.'
}
catch {
    Write-Warning "STOP: $($_.Exception.Message)"
    exit 1
}
