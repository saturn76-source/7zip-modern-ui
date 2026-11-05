@echo off
rem Builds installer\7-zip.chm from the help\ folder.
rem Needs hhc.exe from Microsoft HTML Help Workshop. Set HHC to its path if it is not in the default folder.
rem If hhc says "Itircl.dll has not been registered", run as admin: regsvr32 %WINDIR%\SysWOW64\itircl.dll

setlocal
if "%HHC%"=="" set "HHC=%ProgramFiles(x86)%\HTML Help Workshop\hhc.exe"
if not exist "%HHC%" (
  echo hhc.exe not found: %HHC%
  echo Install HTML Help Workshop or set HHC=path\to\hhc.exe
  exit /b 1
)
pushd "%~dp0help"
"%HHC%" 7zip.hhp
popd
if not exist "%~dp0help\7-zip.chm" (
  echo Help build failed.
  exit /b 1
)
move /y "%~dp0help\7-zip.chm" "%~dp0installer\7-zip.chm" >nul
echo OK: %~dp0installer\7-zip.chm
exit /b 0
