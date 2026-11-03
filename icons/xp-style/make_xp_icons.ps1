# Makes "Modern 4" (Windows XP style) toolbar strips tb4_*.bmp and EasyExtract4.ico
# in CPP\7zip\UI\FileManager. Uses only .NET System.Drawing (no Python needed).
#   powershell -ExecutionPolicy Bypass -File icons\xp-style\make_xp_icons.ps1 [-Preview out.png]
param([string]$Preview)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$fm = Join-Path $here '..\..\CPP\7zip\UI\FileManager' | Resolve-Path
Add-Type -Path (Join-Path $here 'AlphaBmp.cs'), (Join-Path $here 'XpIcons.cs') -ReferencedAssemblies System.Drawing

$files = @()
foreach ($size in 16, 24, 32, 48, 64, 96) {
  $f = Join-Path $fm "tb4_$size.bmp"
  $strip = [XpIcons]::RenderStrip($size)
  [AlphaBmp]::Write($strip, $f)
  $strip.Dispose()
  $files += $f
  "tb4_$size.bmp"
}
# picture in Easy Extract dialog: "Wizard" icon (index 6)
[XpIcons]::WriteIco(6, [int[]](16, 24, 32, 48, 64, 96, 128, 256), (Join-Path $fm 'EasyExtract4.ico'))
'EasyExtract4.ico'

if ($Preview) {
  [array]::Reverse($files)
  [AlphaBmp]::Preview($Preview, [string[]]$files, 1)
}
