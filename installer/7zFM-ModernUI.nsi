; Installer for 7-Zip Modern UI edition (7zFM.exe, x64).
; 7-Zip Copyright (C) Igor Pavlov. GNU LGPL. UI modifications: saturn76-source.
; Build with build_installer.cmd (NSIS 3, makensis).

Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

!define APP_NAME     "7-Zip Modern UI"
!define APP_VERSION  "1.0"
!define BASE_VERSION "26.05"
!define APP_PUBLISHER "saturn76-source (based on 7-Zip by Igor Pavlov)"
!define UNINST_KEY   "Software\Microsoft\Windows\CurrentVersion\Uninstall\7-Zip Modern UI"
!define SRC          ".."
!define FM_EXE       "${SRC}\CPP\7zip\Bundles\Fm\x64\7zFM.exe"

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
!define MUI_FINISHPAGE_RUN "$INSTDIR\7zFM.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run ${APP_NAME}"

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
  File "${SRC}\DOC\License.txt"
  File "${SRC}\DOC\readme.txt"
  File "${SRC}\DOC\copying.txt"
  File "${SRC}\DOC\unRarLicense.txt"

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
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Add ${APP_NAME} to the Start menu."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Add a ${APP_NAME} icon to the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  ; Settings in HKCU\Software\7-Zip\FM are shared with original 7-Zip, so they are kept.
  SetShellVarContext all
  Delete "$SMPROGRAMS\${APP_NAME}.lnk"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  Delete "$INSTDIR\7zFM.exe"
  Delete "$INSTDIR\License.txt"
  Delete "$INSTDIR\readme.txt"
  Delete "$INSTDIR\copying.txt"
  Delete "$INSTDIR\unRarLicense.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
