@echo off
rem Installs the Worship Keys VST3 so FL Studio (or any DAW) can find it.
rem Copies the plugin to Common Files\VST3 and the piano samples to ProgramData.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Asking Windows for permission to install...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set "SRC=%~dp0"
set "VSTDEST=%CommonProgramFiles%\VST3"
set "SAMPLEDEST=%ProgramData%\WorshipKeys\Samples"

echo Copying piano samples to %SAMPLEDEST% ...
robocopy "%SRC%Samples" "%SAMPLEDEST%" /E /NFL /NDL /NJH /NJS /NP >nul
if %errorlevel% geq 8 goto fail

echo Copying plugin to %VSTDEST% ...
robocopy "%SRC%VST3\Worship Keys.vst3" "%VSTDEST%\Worship Keys.vst3" /E /NFL /NDL /NJH /NJS /NP >nul
if %errorlevel% geq 8 goto fail

echo.
echo Done! In FL Studio: Options ^> Manage plugins ^> Find installed plugins,
echo then look for "Worship Keys" under Generators.
echo.
pause
exit /b 0

:fail
echo.
echo Something went wrong while copying. Make sure FL Studio is closed and try again.
pause
exit /b 1
