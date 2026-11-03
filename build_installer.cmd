@echo off
rem Builds the setup exe from installer\7zFM-ModernUI.nsi with NSIS.
rem Run build_7zFM.cmd first.
setlocal
set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
if not exist "%MAKENSIS%" set "MAKENSIS=%ProgramFiles%\NSIS\makensis.exe"
if not exist "%MAKENSIS%" (
  echo NSIS was not found. Install it from https://nsis.sourceforge.io
  pause
  exit /b 1
)
if not exist "%~dp0CPP\7zip\Bundles\Fm\x64\7zFM.exe" (
  echo 7zFM.exe not found. Run build_7zFM.cmd first.
  pause
  exit /b 1
)
cd /d "%~dp0installer"
"%MAKENSIS%" /V2 7zFM-ModernUI.nsi
if errorlevel 1 (
  echo INSTALLER BUILD FAILED
  pause
  exit /b 1
)
echo.
echo OK: %CD%\7zFM-ModernUI-1.0-x64-setup.exe
pause
