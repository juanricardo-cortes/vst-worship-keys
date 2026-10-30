# Installs Worship Keys: samples -> C:\ProgramData\WorshipKeys\Samples, VST3 -> Common Files\VST3 (needs admin).
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$vst3 = Join-Path $root "build\WorshipKeys_artefacts\Release\VST3\Worship Keys.vst3"
$samples = Join-Path $root "Samples"
$sampleDest = Join-Path $env:ProgramData "WorshipKeys\Samples"
$vstDest = Join-Path $env:CommonProgramFiles "VST3"

if (-not (Test-Path $vst3)) { throw "Build first: cmake --build build --config Release" }

New-Item -ItemType Directory -Force $sampleDest | Out-Null
Copy-Item -Path (Join-Path $samples "*") -Destination $sampleDest -Recurse -Force
Write-Host "Samples -> $sampleDest"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$copy = "New-Item -ItemType Directory -Force '$vstDest' | Out-Null; Copy-Item -Path '$vst3' -Destination '$vstDest' -Recurse -Force"
if ($isAdmin) { Invoke-Expression $copy }
else { Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile", "-Command", $copy }

if (Test-Path (Join-Path $vstDest "Worship Keys.vst3")) { Write-Host "VST3 -> $vstDest\Worship Keys.vst3" }
else { throw "VST3 copy failed (admin prompt declined?)" }
