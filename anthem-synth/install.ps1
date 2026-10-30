# Installs Anthem Synth: VST3 -> Common Files\VST3 (needs admin), plus a Start-menu shortcut to the standalone app.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$vst3 = Join-Path $root "build\AnthemSynth_artefacts\Release\VST3\Anthem Synth.vst3"
$exe = Join-Path $root "build\AnthemSynth_artefacts\Release\Standalone\Anthem Synth.exe"
$vstDest = Join-Path $env:CommonProgramFiles "VST3"

if (-not (Test-Path $vst3)) { throw "Build first: cmake --build build --config Release" }

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$copy = "New-Item -ItemType Directory -Force '$vstDest' | Out-Null; Copy-Item -Path '$vst3' -Destination '$vstDest' -Recurse -Force"
if ($isAdmin) { Invoke-Expression $copy }
else { Start-Process powershell -Verb RunAs -Wait -ArgumentList "-NoProfile", "-Command", $copy }

if (Test-Path (Join-Path $vstDest "Anthem Synth.vst3")) { Write-Host "VST3 -> $vstDest\Anthem Synth.vst3" }
else { throw "VST3 copy failed (admin prompt declined?)" }

$startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$shell = New-Object -ComObject WScript.Shell
$lnk = $shell.CreateShortcut((Join-Path $startMenu "Anthem Synth.lnk"))
$lnk.TargetPath = $exe
$lnk.WorkingDirectory = Split-Path $exe
$lnk.Save()
Write-Host "Start menu shortcut -> Anthem Synth"
