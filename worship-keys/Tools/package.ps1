# Builds dist\WorshipKeys-Portable.zip: standalone exe + VST3 + samples + installer .bat.
# Build first: cmake --build build --config Release
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$art = Join-Path $root "build\WorshipKeys_artefacts\Release"
$dist = Join-Path $root "dist"
$stage = Join-Path $dist "Worship Keys"
$zip = Join-Path $dist "WorshipKeys-Portable.zip"

$exe = Join-Path $art "Standalone\Worship Keys.exe"
$vst3 = Join-Path $art "VST3\Worship Keys.vst3"
if (-not (Test-Path $exe) -or -not (Test-Path $vst3)) { throw "Build first: cmake --build build --config Release" }

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage "VST3") | Out-Null

Copy-Item $exe $stage
Copy-Item $vst3 (Join-Path $stage "VST3") -Recurse
Copy-Item (Join-Path $root "Samples") $stage -Recurse
Copy-Item (Join-Path $PSScriptRoot "portable\*") $stage

if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Wrote $zip"
