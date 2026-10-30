# Installs Worship Station:
#   VST3            -> C:\Program Files\Common Files\VST3
#   orchestra samples -> C:\ProgramData\WorshipStation\Samples\Orchestra
#   piano samples   -> C:\ProgramData\WorshipKeys\Samples (only if Worship Keys hasn't put them there already)
#   Start-menu shortcut to the standalone app.
# Needs admin for the VST3 / ProgramData copies (you'll get one UAC prompt).
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$vst3 = Join-Path $root "build\WorshipStation_artefacts\Release\VST3\Worship Station.vst3"
$exe = Join-Path $root "build\WorshipStation_artefacts\Release\Standalone\Worship Station.exe"
$orchSamples = Join-Path $root "Samples\Orchestra"
$pianoSamples = Join-Path $root "..\worship-keys\Samples"
$vstDest = Join-Path $env:CommonProgramFiles "VST3"
$orchDest = Join-Path $env:ProgramData "WorshipStation\Samples\Orchestra"
$pianoDest = Join-Path $env:ProgramData "WorshipKeys\Samples"

if (-not (Test-Path $vst3)) { throw "Build first: cmake --build build --config Release" }
if (-not (Test-Path (Join-Path $orchSamples "VlnStac\map.txt"))) { throw "Orchestra samples missing: python Tools\prepare_orchestra.py" }

$copy = @"
New-Item -ItemType Directory -Force '$vstDest' | Out-Null
Copy-Item -Path '$vst3' -Destination '$vstDest' -Recurse -Force
New-Item -ItemType Directory -Force '$orchDest' | Out-Null
Copy-Item -Path '$orchSamples\*' -Destination '$orchDest' -Recurse -Force
if (-not (Test-Path '$pianoDest\tuning.txt') -and (Test-Path '$pianoSamples')) {
    New-Item -ItemType Directory -Force '$pianoDest' | Out-Null
    Copy-Item -Path '$pianoSamples\*' -Destination '$pianoDest' -Recurse -Force
}
"@

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($isAdmin) { Invoke-Expression $copy }
else { Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile", "-Command", $copy }

if (Test-Path (Join-Path $vstDest "Worship Station.vst3")) { Write-Host "VST3 -> $vstDest\Worship Station.vst3" }
else { throw "VST3 copy failed (admin prompt declined?)" }
if (Test-Path (Join-Path $orchDest "VlnStac\map.txt")) { Write-Host "Orchestra samples -> $orchDest" }
if (Test-Path (Join-Path $pianoDest "tuning.txt")) { Write-Host "Piano samples -> $pianoDest" }

$startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$shell = New-Object -ComObject WScript.Shell
$lnk = $shell.CreateShortcut((Join-Path $startMenu "Worship Station.lnk"))
$lnk.TargetPath = $exe
$lnk.WorkingDirectory = Split-Path $exe
$lnk.Save()
Write-Host "Start menu shortcut -> Worship Station"
