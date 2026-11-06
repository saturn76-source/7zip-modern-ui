@echo off
rem Builds the right-click (context menu) parts of 7-Zip Modern UI edition (x64):
rem   7-zip.dll  - Explorer context menu      (CPP\7zip\UI\Explorer)
rem   7zG.exe    - compress / extract windows (CPP\7zip\UI\GUI)
rem   7z.dll     - archive formats for 7zG    (CPP\7zip\Bundles\Format7zF)
rem Each part is built in its own x64 folder next to its makefile.
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
  echo Visual Studio with C++ tools was not found.
  exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

for %%d in ("Bundles\Format7zF" "UI\GUI" "UI\Explorer") do (
  echo.
  echo ===== %%~d =====
  pushd "%~dp0CPP\7zip\%%~d"
  nmake /nologo PLATFORM=x64
  if errorlevel 1 (
    popd
    echo BUILD FAILED: %%~d
    exit /b 1
  )
  popd
)
echo.
echo OK: 7z.dll, 7zG.exe, 7-zip.dll
exit /b 0
