7-Zip File Manager with WinRAR-style toolbar
============================================

BUILD IN VISUAL STUDIO 2026
  1. Visual Studio Installer: "Desktop development with C++" must be installed.
  2. Open 7zFM-Toolbar.sln
  3. Build > Build Solution  (Release | x64)
  4. Result: CPP\7zip\Bundles\Fm\x64\7zFM.exe   (standalone, no 7z.dll needed)
     Press F5 / Ctrl+F5 to run it.

BUILD FROM COMMAND LINE
  Run build_7zFM.cmd  (finds Visual Studio automatically)

WHAT WAS CHANGED
  CPP\7zip\UI\FileManager\App.cpp      one toolbar, big icons with text under them
  CPP\7zip\UI\FileManager\App.h        defaults: big buttons + text; one on/off switch
  CPP\7zip\UI\FileManager\MyLoadMenu.cpp  View > Toolbars > "Toolbar" check mark
  CPP\7zip\UI\FileManager\resource.rc/.h  new strings, toolbar images, menu
  CPP\7zip\UI\FileManager\tb24.bmp ... tb96.bmp  toolbar images (32-bit with alpha)

BUTTONS (edit table g_Buttons[] in App.cpp to change them)
  7ZIP logo -> About 7-Zip
  Add       -> Add to archive
  Extract   -> Extract
  Test      -> Test archive
  View      -> View file (F3)
  Delete    -> Delete (Del)
  Find      -> Find files in this folder and subfolders (Edit > Find..., Ctrl+F)
  Wizard    -> Options
  Info      -> Properties (Alt+Enter)
  Comment   -> File > Comment (Ctrl+Z)

View > Toolbars: "Toolbar" shows/hides it, "Large Buttons" switches 48/32 px icons,
"Show Buttons Text" shows/hides the labels. Icon size also follows Windows DPI scaling.
