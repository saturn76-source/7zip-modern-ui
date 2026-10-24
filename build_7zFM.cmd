@echo off
rem Builds 7zFM.exe (x64) with Visual Studio C++ tools.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
  echo Visual Studio with C++ tools was not found.
  echo Install "Desktop development with C++" in Visual Studio Installer.
  pause
  exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "%~dp0CPP\7zip\Bundles\Fm"
nmake /nologo PLATFORM=x64
if errorlevel 1 (
  echo BUILD FAILED
  pause
  exit /b 1
)
echo.
echo OK: %CD%\x64\7zFM.exe
pause
