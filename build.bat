@echo off
setlocal
cd /d "%~dp0"
echo.
echo  === Building Afterhours VST3 ===
echo.

rem --- Find Visual Studio (needs the "Desktop development with C++" workload) ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto novs
call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul

where cmake >nul 2>nul || goto nocmake

rem --- Configure + build (first run downloads JUCE, ~5-10 min total) ---
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || goto failed
cmake --build build --config Release || goto failed

set "OUT=%~dp0build\Afterhours_artefacts\Release\VST3\Afterhours.vst3"
if not exist "%OUT%" goto failed
echo.
echo  Built: %OUT%
echo  Standalone app: %~dp0build\Afterhours_artefacts\Release\Standalone\Afterhours.exe
echo.

rem --- Install into the standard VST3 folder (needs admin rights) ---
xcopy "%OUT%" "%CommonProgramFiles%\VST3\Afterhours.vst3\" /E /I /Y /Q >nul 2>nul
if errorlevel 1 (
    echo  Could not copy into "%CommonProgramFiles%\VST3" - right-click build.bat and choose
    echo  "Run as administrator", or copy the Afterhours.vst3 folder there yourself.
    explorer "%~dp0build\Afterhours_artefacts\Release\VST3"
) else (
    echo  Installed to "%CommonProgramFiles%\VST3\Afterhours.vst3"
    echo  Rescan plugins in your DAW and load Afterhours as an instrument.
)
echo.
pause
exit /b 0

:novs
echo  Visual Studio 2022 or newer with the "Desktop development with C++" workload was not found.
echo  Install the free Community edition: https://visualstudio.microsoft.com/vs/community/
pause
exit /b 1

:nocmake
echo  CMake was not found. In the Visual Studio Installer, click Modify and make sure
echo  "C++ CMake tools for Windows" is ticked.
pause
exit /b 1

:failed
echo.
echo  Build failed - scroll up for the first error message.
pause
exit /b 1
