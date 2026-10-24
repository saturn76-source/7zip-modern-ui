// DarkMode.h
// Light / Dark / Auto (follow Windows) theme for 7-Zip File Manager

#ifndef ZIP7_INC_FM_DARK_MODE_H
#define ZIP7_INC_FM_DARK_MODE_H

#include "../../../Common/MyWindows.h"

namespace NDarkMode {

enum EThemeMode
{
  k_Theme_Light = 0,
  k_Theme_Dark  = 1,
  k_Theme_Auto  = 2  // follow Windows setting "Choose your app mode"
};

// reads saved mode from registry and calculates current state
void Init();

EThemeMode GetMode();

// saves mode to registry; returns true, if dark/light state was changed
bool SetMode(EThemeMode mode);

// true, if dark colors must be used now
bool IsDark();

// for Auto mode: re-reads Windows setting. returns true, if state was changed
bool UpdateFromSystem();

// WM_SETTINGCHANGE: is it "ImmersiveColorSet" (Windows app mode was changed)?
bool IsThemeSettingChange(LPARAM lParam);

COLORREF Color_Bk();
COLORREF Color_Text();
COLORREF Color_ListBk();
COLORREF Color_Hot();
COLORREF Color_SelectMark(); // background of items selected with Insert key

HBRUSH Brush_Bk();
HBRUSH Brush_ListBk();
HBRUSH Brush_EditBk();

// main window: title bar, menu bar, popup menus
void ApplyToMainWindow(HWND hwnd);

// title bar of top level window (main window, dialogs)
void ApplyToTitleBar(HWND hwnd);

void ApplyToListView(HWND listView);
void ApplyToComboBoxEx(HWND comboBoxEx);
void ApplyToStatusBar(HWND statusBar);
void ApplyToBkWindow(HWND hwnd); // rebar and other containers: dark background

// main window: draws dark menu bar (undocumented WM_UAHDRAWMENU messages)
bool OnMainWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT &result);

// hook for all dialogs and property pages (see Windows/Control/Dialog.cpp)
bool DialogHook(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT &result);

}

#endif
