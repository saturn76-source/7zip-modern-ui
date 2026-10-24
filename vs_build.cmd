@echo off
rem Used by Visual Studio "Build" (7zFM-Toolbar.vcxproj).
rem 7-Zip makefile does not track .h / .rc / .bmp changes,
rem so if any of them is newer than 7zFM.exe, we do a full rebuild.
setlocal
set "ROOT=%~dp0"
cd /d "%ROOT%CPP\7zip\Bundles\Fm"
if not exist x64\7zFM.exe goto build
powershell -NoProfile -ExecutionPolicy Bypass -Command "$e=(Get-Item 'x64\7zFM.exe').LastWriteTime; $n=@(Get-ChildItem -LiteralPath ($env:ROOT+'CPP'),($env:ROOT+'C') -Recurse -File -Include *.h,*.rc,*.bmp,*.ico,*.manifest | Where-Object { $_.LastWriteTime -gt $e }); if ($n.Count -gt 0) { Write-Host ('Changed: ' + $n[0].FullName); exit 1 }; exit 0"
if errorlevel 1 (
  echo Headers or resources changed: doing full rebuild...
  rmdir /s /q x64
)
:build
nmake /nologo PLATFORM=x64
