# Installs the dcsswiftbus export block into every DCS Saved Games profile.
# Idempotent: re-running replaces the previous dcsswiftbus block, everything
# else in an existing Export.lua (SRS, TacView, Helios, ...) is left untouched.
# A backup (Export.lua.dcsswiftbus.bak) is written before any change.

$ErrorActionPreference = "Stop"

$BeginMarker = "-- ===== BEGIN dcsswiftbus (auto-installed, do not edit this block) ====="
$EndMarker = "-- ===== END dcsswiftbus ====="

# our Export.lua: next to this script (packaged layout) or ../dcs/ (repo layout)
$candidates = @(
    (Join-Path $PSScriptRoot "Export.lua"),
    (Join-Path $PSScriptRoot "..\dcs\Export.lua")
)
$sourceFile = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $sourceFile) {
    Write-Host "ERROR: could not find the dcsswiftbus Export.lua next to this script." -ForegroundColor Red
    exit 1
}
$block = $BeginMarker + "`r`n" + (Get-Content $sourceFile -Raw).TrimEnd() + "`r`n" + $EndMarker

$savedGames = Join-Path $env:USERPROFILE "Saved Games"
$profiles = Get-ChildItem -Path $savedGames -Directory -Filter "DCS*" -ErrorAction SilentlyContinue
if (-not $profiles) {
    Write-Host "ERROR: no 'DCS*' folder found in $savedGames - has DCS been run at least once?" -ForegroundColor Red
    exit 1
}

foreach ($profile in $profiles) {
    $scriptsDir = Join-Path $profile.FullName "Scripts"
    if (-not (Test-Path $scriptsDir)) { New-Item -ItemType Directory -Path $scriptsDir | Out-Null }
    $exportFile = Join-Path $scriptsDir "Export.lua"

    $existing = ""
    if (Test-Path $exportFile) {
        $existing = Get-Content $exportFile -Raw
        Copy-Item $exportFile "$exportFile.dcsswiftbus.bak" -Force
        # remove a previously installed dcsswiftbus block
        $pattern = "(?s)\r?\n?" + [regex]::Escape($BeginMarker) + ".*?" + [regex]::Escape($EndMarker) + "\r?\n?"
        $existing = ([regex]::Replace($existing, $pattern, "`r`n")).TrimEnd()
    }

    $content = if ($existing) { $existing + "`r`n`r`n" + $block + "`r`n" } else { $block + "`r`n" }
    Set-Content -Path $exportFile -Value $content -NoNewline
    Write-Host "OK: installed into $exportFile" -ForegroundColor Green

    if (-not (Test-Path (Join-Path $profile.FullName "Mods\Services\DCS-SRS"))) {
        Write-Host "    note: DCS-SRS not found in this profile - cockpit radio sync will be" -ForegroundColor Yellow
        Write-Host "    unavailable (swift GUI radios still work). Install SRS to enable it." -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Done. Restart DCS if it is running."
