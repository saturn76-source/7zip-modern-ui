; Installer for 7-Zip Modern UI edition (x64): 7zFM.exe + Explorer context menu (7-zip.dll, 7zG.exe, 7z.dll).
; 7-Zip Copyright (C) Igor Pavlov. GNU LGPL. UI modifications: saturn76-source.
; Build with build_installer.cmd (NSIS 3, makensis).

Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

!define APP_NAME     "7-Zip Modern UI"
!define APP_VERSION  "1.01"
!define BASE_VERSION "26.05"
!define APP_PUBLISHER "saturn76-source (based on 7-Zip by Igor Pavlov)"
!define UNINST_KEY   "Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip Modern UI"
!define SRC          ".."
!define FM_EXE       "${SRC}\CPP\7zip\Bundles\Fm\x64\7zFM.exe"
!define G_EXE        "${SRC}\CPP\7zip\UI\GUI\x64\7zG.exe"
!define FORMATS_DLL  "${SRC}\CPP\7zip\Bundles\Format7zF\x64\7z.dll"
!define SHELL_DLL    "${SRC}\CPP\7zip\UI\Explorer\x64\7-zip.dll"

; Explorer context menu: own CLSID and handler name (see UI\Explorer\RegistryContextMenu.cpp),
; so it works side by side with original 7-Zip.
!define SHELL_CLSID  "{EE1D8B6E-4E13-46F0-A8CB-AD5F6BE235A6}"
!define SHELL_NAME   "7-Zip Modern UI"
!define CLASSES      "Software\Classes"
!define APPROVED_KEY "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "7zFM-ModernUI-${APP_VERSION}-x64-setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "${UNINST_KEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ManifestDPIAware true

VIProductVersion "${APP_VERSION}.0.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${APP_VERSION} (7-Zip ${BASE_VERSION})"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "FileDescription" "${APP_NAME} Setup"
VIAddVersionKey "LegalCopyright" "Copyright (c) Igor Pavlov. GNU LGPL."
VIAddVersionKey "CompanyName" "saturn76-source"

!define MUI_ICON   "${SRC}\CPP\7zip\UI\FileManager\FM.ico"
!define MUI_UNICON "${SRC}\CPP\7zip\UI\FileManager\FM.ico"
!define MUI_ABORTWARNING
; side picture of Welcome / Finish pages (drawn at 150%: 246x471, NSIS scales it)
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\7zFM.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run ${APP_NAME}"
; shown only when a file was in use (Explorer had the old 7-zip.dll loaded)
!define MUI_FINISHPAGE_TEXT_REBOOT "${APP_NAME} ${APP_VERSION} is installed and ready to use.$\r$\n$\r$\nWindows Explorer was using the old right-click menu file (7-zip.dll), so it could not be deleted now. The old file will be deleted when you restart the computer. You can restart later.$\r$\n$\r$\nUntil then, the right-click menu in Explorer can still be the old one."
!define MUI_FINISHPAGE_REBOOTLATER_DEFAULT

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SRC}\DOC\License.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "${APP_NAME} needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "${APP_NAME} (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${FM_EXE}"
  File "${G_EXE}"
  File "${FORMATS_DLL}"
  ; Explorer may have the old 7-zip.dll loaded: a loaded DLL can be renamed, but not overwritten.
  Delete /REBOOTOK "$INSTDIR\7-zip.dll.old"
  Rename "$INSTDIR\7-zip.dll" "$INSTDIR\7-zip.dll.old"
  Delete /REBOOTOK "$INSTDIR\7-zip.dll.old"
  File "${SHELL_DLL}"
  File "${SRC}\DOC\License.txt"
  File "${SRC}\DOC\readme.txt"
  File "${SRC}\DOC\copying.txt"
  File "${SRC}\DOC\unRarLicense.txt"
  ; help file for Help > Contents (F1), taken from original 7-Zip 26.05 installer
  File "7-zip.chm"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayName" "${APP_NAME} ${APP_VERSION} (x64)"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr   HKLM "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr   HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\7zFM.exe"
  WriteRegStr   HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   HKLM "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Explorer context menu" SecContextMenu
  WriteRegStr HKLM "${CLASSES}\CLSID\${SHELL_CLSID}" "" "${SHELL_NAME} Shell Extension"
  WriteRegStr HKLM "${CLASSES}\CLSID\${SHELL_CLSID}\InprocServer32" "" "$INSTDIR\7-zip.dll"
  WriteRegStr HKLM "${CLASSES}\CLSID\${SHELL_CLSID}\InprocServer32" "ThreadingModel" "Apartment"
  WriteRegStr HKLM "${CLASSES}\*\shellex\ContextMenuHandlers\${SHELL_NAME}" "" "${SHELL_CLSID}"
  WriteRegStr HKLM "${CLASSES}\Folder\shellex\ContextMenuHandlers\${SHELL_NAME}" "" "${SHELL_CLSID}"
  WriteRegStr HKLM "${CLASSES}\Directory\shellex\ContextMenuHandlers\${SHELL_NAME}" "" "${SHELL_CLSID}"
  WriteRegStr HKLM "${CLASSES}\Directory\shellex\DragDropHandlers\${SHELL_NAME}" "" "${SHELL_CLSID}"
  WriteRegStr HKLM "${CLASSES}\Drive\shellex\DragDropHandlers\${SHELL_NAME}" "" "${SHELL_CLSID}"
  WriteRegStr HKLM "${APPROVED_KEY}" "${SHELL_CLSID}" "${SHELL_NAME} Shell Extension"
  ; SHCNE_ASSOCCHANGED: tell Explorer to reload shell extensions
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Start Menu shortcut" SecStartMenu
  SetShellVarContext all
  CreateShortcut "$SMPROGRAMS\${APP_NAME}.lnk" "$INSTDIR\7zFM.exe"
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  SetShellVarContext all
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\7zFM.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "7-Zip File Manager with the Modern UI."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecContextMenu} "Add the ${APP_NAME} menu (Add to archive, Extract, CRC SHA) to the right-click menu in Explorer. In Windows 11 it is under Show more options."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Add ${APP_NAME} to the Start menu."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Add a ${APP_NAME} icon to the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  ; User settings in HKCU\Software\7-Zip Modern UI (own key, not shared with original 7-Zip)
  ; are kept, so a reinstall keeps the user's toolbar, theme and zoom.
  SetShellVarContext all
  ; Explorer context menu
  DeleteRegKey HKLM "${CLASSES}\CLSID\${SHELL_CLSID}"
  DeleteRegKey HKLM "${CLASSES}\*\shellex\ContextMenuHandlers\${SHELL_NAME}"
  DeleteRegKey HKLM "${CLASSES}\Folder\shellex\ContextMenuHandlers\${SHELL_NAME}"
  DeleteRegKey HKLM "${CLASSES}\Directory\shellex\ContextMenuHandlers\${SHELL_NAME}"
  DeleteRegKey HKLM "${CLASSES}\Directory\shellex\DragDropHandlers\${SHELL_NAME}"
  DeleteRegKey HKLM "${CLASSES}\Drive\shellex\DragDropHandlers\${SHELL_NAME}"
  DeleteRegValue HKLM "${APPROVED_KEY}" "${SHELL_CLSID}"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
  ; Explorer may still have 7-zip.dll loaded: then it is deleted after restart
  Delete /REBOOTOK "$INSTDIR\7-zip.dll"
  Delete /REBOOTOK "$INSTDIR\7-zip.dll.old"
  Delete "$INSTDIR\7zG.exe"
  Delete "$INSTDIR\7z.dll"
  Delete "$SMPROGRAMS\${APP_NAME}.lnk"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  Delete "$INSTDIR\7zFM.exe"
  Delete "$INSTDIR\License.txt"
  Delete "$INSTDIR\readme.txt"
  Delete "$INSTDIR\copying.txt"
  Delete "$INSTDIR\unRarLicense.txt"
  Delete "$INSTDIR\7-zip.chm"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /REBOOTOK "$INSTDIR"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
