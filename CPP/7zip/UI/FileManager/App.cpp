// App.cpp

#include "StdAfx.h"

#include <shellapi.h>

#include "resource.h"
#include "OverwriteDialogRes.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariantConv.h"

/*
#include "Windows/COM.h"
#include "Windows/Error.h"
#include "Windows/FileDir.h"

#include "Windows/PropVariant.h"
#include "Windows/Thread.h"
*/

#include "App.h"
#include "DarkMode.h"
#include "ToolbarCustomizeDialog.h"

// these toolbar custom draw flags require (_WIN32_IE >= 0x0600) in commctrl.h
#ifndef TBCDRF_USECDCOLORS
#define TBCDRF_USECDCOLORS 0x00800000
#endif
#ifndef TBCDRF_HILITEHOTTRACK
#define TBCDRF_HILITEHOTTRACK 0x00020000
#endif
#include "CopyDialog.h"
#include "ExtractCallback.h"
#include "FormatUtils.h"
#include "IFolder.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"
#include "RegistryUtils.h"
#include "ViewSettings.h"

#include "PropertyNameRes.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;
using namespace NFind;
using namespace NName;

extern HINSTANCE g_hInstance;

#define kTempDirPrefix FTEXT("7zE")

void CPanelCallbackImp::OnTab()
{
  if (g_App.NumPanels != 1)
    _app->Panels[1 - _index].SetFocusToList();
  _app->RefreshTitle();
}

void CPanelCallbackImp::SetFocusToPath(unsigned index)
{
  unsigned newPanelIndex = index;
  if (g_App.NumPanels == 1)
    newPanelIndex = g_App.LastFocusedPanel;
  _app->RefreshTitle();
  _app->Panels[newPanelIndex]._headerComboBox.SetFocus();
  _app->Panels[newPanelIndex]._headerComboBox.ShowDropDown();
}


void CPanelCallbackImp::OnCopy(bool move, bool copyToSame) { _app->OnCopy(move, copyToSame, _index); }
void CPanelCallbackImp::OnSetSameFolder() { _app->OnSetSameFolder(_index); }
void CPanelCallbackImp::OnSetSubFolder()  { _app->OnSetSubFolder(_index); }
void CPanelCallbackImp::PanelWasFocused() { _app->SetFocusedPanel(_index); _app->RefreshTitlePanel(_index); }
void CPanelCallbackImp::DragBegin() { _app->DragBegin(_index); }
void CPanelCallbackImp::DragEnd() { _app->DragEnd(); }
void CPanelCallbackImp::RefreshTitle(bool always) { _app->RefreshTitlePanel(_index, always); }

void CApp::ReloadLangItems()
{
  LangString(IDS_N_SELECTED_ITEMS, LangString_N_SELECTED_ITEMS);
}

void CApp::SetListSettings()
{
  CFmSettings st;
  st.Load();

  ShowSystemMenu = st.ShowSystemMenu;

  DWORD extendedStyle = LVS_EX_HEADERDRAGDROP;
  if (st.FullRow)
    extendedStyle |= LVS_EX_FULLROWSELECT;
  if (st.ShowGrid)
    extendedStyle |= LVS_EX_GRIDLINES;
  
  if (st.SingleClick)
  {
    extendedStyle |= LVS_EX_ONECLICKACTIVATE | LVS_EX_TRACKSELECT;
    /*
    if (ReadUnderline())
      extendedStyle |= LVS_EX_UNDERLINEHOT;
    */
  }

  for (unsigned i = 0; i < kNumPanelsMax; i++)
  {
    CPanel &panel = Panels[i];
    panel._mySelectMode = st.AlternativeSelection;
    panel._showDots = st.ShowDots;
    panel._showRealFileIcons = st.ShowRealFileIcons;
    panel._exStyle = extendedStyle;

    LONG_PTR style = panel._listView.GetStyle();
    if (st.AlternativeSelection)
      style |= LVS_SINGLESEL;
    else
      style &= ~(LONG_PTR)(DWORD)LVS_SINGLESEL;
    panel._listView.SetStyle(style);
    panel.SetExtendedStyle();
  }
}

#ifndef ILC_COLOR32
#define ILC_COLOR32 0x0020
#endif

HRESULT CApp::CreateOnePanel(unsigned panelIndex, const UString &mainPath, const UString &arcFormat,
    bool needOpenArc,
    COpenResult &openRes)
{
  if (Panels[panelIndex].PanelCreated)
    return S_OK;
  
  m_PanelCallbackImp[panelIndex].Init(this, panelIndex);
  
  UString path;
  if (mainPath.IsEmpty())
  {
    if (!::ReadPanelPath(panelIndex, path))
      path.Empty();
  }
  else
    path = mainPath;
  
  const unsigned id = 1000 + 100 * panelIndex; // check it

  const HRESULT res = Panels[panelIndex].Create(_window, _window,
      id, path, arcFormat, &m_PanelCallbackImp[panelIndex], &AppState,
      needOpenArc,
      openRes);
  Panels[panelIndex].ApplyTheme();
  return res;
}

void CApp::ApplyTheme()
{
  if (!_window)
    return;
  NDarkMode::ApplyToMainWindow(_window);
  for (unsigned i = 0; i < kNumPanelsMax; i++)
    if (Panels[i].PanelCreated)
      Panels[i].ApplyTheme();
  ::RedrawWindow(_window, NULL, NULL,
      RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void CApp::SetThemeMode(int mode)
{
  NDarkMode::SetMode((NDarkMode::EThemeMode)mode);
  // ApplyTheme() also for same dark/light state: it's cheap and it refreshes everything
  ApplyTheme();
}


/*
  WinRAR-style toolbar:
    one toolbar, big colored icons with the text under each icon.
  Images are stored in 32-bit (alpha) bitmap strips: tb24.bmp ... tb96.bmp.
  Every strip has (kNumToolbarImages) square cells of size (height) x (height).
  Cell order in strip: Add, Extract, Test, View, Delete, Find, Wizard, Info, Comment, Copy, Move
  The 7ZIP logo (logo24.bmp ... logo96.bmp) is not a button:
  it's drawn at the left side of toolbar, in the indent area.
*/

static const unsigned kNumToolbarImages = 11;

struct CToolbarStripInfo
{
  unsigned Height;
  UINT ResID;
  UINT LogoResID;
};

static const CToolbarStripInfo g_ToolbarStrips[] =
{
  { 24, IDB_TB24, IDB_LOGO24 },
  { 32, IDB_TB32, IDB_LOGO32 },
  { 40, IDB_TB40, IDB_LOGO40 },
  { 48, IDB_TB48, IDB_LOGO48 },
  { 64, IDB_TB64, IDB_LOGO64 },
  { 72, IDB_TB72, IDB_LOGO72 },
  { 96, IDB_TB96, IDB_LOGO96 }
};

static HBITMAP LoadAlphaBitmap(UINT resID)
{
  // 32-bit bitmap with alpha channel.
  // LR_CREATEDIBSECTION keeps the alpha channel.
  return (HBITMAP)::LoadImage(g_hInstance, MAKEINTRESOURCE(resID),
      IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
}

static unsigned GetScreenDpi()
{
  unsigned dpi = 96;
  HDC dc = ::GetDC(NULL);
  if (dc)
  {
    const int d = ::GetDeviceCaps(dc, LOGPIXELSY);
    if (d > 0)
      dpi = (unsigned)d;
    ::ReleaseDC(NULL, dc);
  }
  return dpi;
}

static const CToolbarStripInfo &GetToolbarStrip(bool largeButtons)
{
  const unsigned want = (largeButtons ? 48 : 32) * GetScreenDpi() / 96;
  unsigned best = 0;
  unsigned bestDiff = (unsigned)(Int32)-1;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_ToolbarStrips); i++)
  {
    const unsigned h = g_ToolbarStrips[i].Height;
    const unsigned diff = (h > want) ? (h - want) : (want - h);
    if (diff < bestDiff)
    {
      bestDiff = diff;
      best = i;
    }
  }
  return g_ToolbarStrips[best];
}

static const int kLogoMargin = 10;

static unsigned CreateToolbar(HWND parent,
    NControl::CImageList &imageList,
    NControl::CImageList & /* logoImageList */,
    NControl::CToolBar &toolBar,
    bool largeButtons,
    bool classic)
{
  toolBar.Attach(::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 0
      | WS_CHILD
      | WS_VISIBLE
      | WS_CLIPSIBLINGS   // don't paint over the ">>" (chevron) button
      | CCS_ADJUSTABLE    // "Customize Toolbar" dialog, Alt+drag to move / remove buttons
      | TBSTYLE_FLAT
      | TBSTYLE_TOOLTIPS
      // | TBSTYLE_WRAPABLE  // one row only (like WinRAR): buttons are not moved to next row
      // | TBSTYLE_AUTOSIZE
      // | CCS_NORESIZE
      #ifdef UNDER_CE
      | CCS_NODIVIDER
      | CCS_NOPARENTALIGN
      #endif
      ,0,0,0,0, parent, NULL, g_hInstance, NULL));

  // TB_BUTTONSTRUCTSIZE message, which is required for
  // backward compatibility.
  toolBar.ButtonStructSize();

  if (classic)
  {
    // "Classic Icons": original 7-Zip icons, 48x36 (large) or 24x24 (small).
    // No logo and default space around buttons, like in original 7-Zip.
    const unsigned w = largeButtons ? 48 : 24;
    const unsigned h = largeButtons ? 36 : 24;
    imageList.Create((int)w, (int)h, ILC_COLOR32, (int)kNumToolbarImages, 0);
    HBITMAP b = LoadAlphaBitmap(largeButtons ? IDB_TBC_L : IDB_TBC_S);
    if (b)
    {
      imageList.Add(b);
      ::DeleteObject(b);
    }
    toolBar.SetImageList(0, imageList);
    return h;
  }

  const CToolbarStripInfo &strip = GetToolbarStrip(largeButtons);
  const unsigned h = strip.Height;

  imageList.Create((int)h, (int)h, ILC_COLOR32, (int)kNumToolbarImages, 0);
  {
    HBITMAP b = LoadAlphaBitmap(strip.ResID);
    if (b)
    {
      imageList.Add(b);
      ::DeleteObject(b);
    }
  }
  toolBar.SetImageList(0, imageList);

  // small space around buttons, like in WinRAR
  {
    const unsigned padX = h / 4 + 4;
    const unsigned padY = h / 10 + 4;
    ::SendMessage(toolBar, TB_SETPADDING, 0, MAKELPARAM(padX, padY));
  }

  // no logo in toolbar (user removed it)
  return h;
}

// draws the logo in the indent area of toolbar (NM_CUSTOMDRAW / CDDS_POSTPAINT)
static void DrawToolbarLogo(HWND toolBar, HIMAGELIST logoImageList, HDC dc)
{
  if (!logoImageList)
    return;
  int w = 0, h = 0;
  if (!ImageList_GetIconSize(logoImageList, &w, &h))
    return;
  RECT rc;
  if (!::GetClientRect(toolBar, &rc))
    return;
  // center the logo to the icons of the buttons (not to the text)
  int y = (rc.bottom - h) / 2;
  RECT br;
  if (::SendMessage(toolBar, TB_GETITEMRECT, 0, (LPARAM)&br))
  {
    int iconW = 0, iconH = 0;
    HIMAGELIST il = (HIMAGELIST)::SendMessage(toolBar, TB_GETIMAGELIST, 0, 0);
    if (il && ImageList_GetIconSize(il, &iconW, &iconH))
    {
      const DWORD pad = (DWORD)::SendMessage(toolBar, TB_GETPADDING, 0, 0);
      const int iconTop = br.top + (int)HIWORD(pad) / 2;
      y = iconTop + (iconH - h) / 2;
    }
  }
  if (y < 0)
    y = 0;
  // at the right end of toolbar
  const int x = (int)rc.right - kLogoMargin - w;
  if (x < 0)
    return;
  ImageList_Draw(logoImageList, 0, dc, x, y, ILD_NORMAL);
}


struct CButtonInfo
{
  int CommandID;
  unsigned ImageIndex;  // cell index in tb*.bmp strip
  UINT StringResID;
  bool ShowText;
  bool SeparatorBefore;
  bool InDefault;       // false: only in "Available buttons" list of Customize dialog

  UString GetText() const { return LangString(StringResID); }
};

/*
  All buttons that can be in toolbar.
  User selects buttons and their order in "Customize Toolbar" dialog.
  The layout is saved in registry: HKCU\Software\7-Zip\FM : ToolbarLayout
  as list of indexes in this table, '-' is separator. For example: "0,1,2,-,8"
  CommandID: any command from main menu (IDM_* in resource.h) or kMenuCmdID_Toolbar_*
*/

static const CButtonInfo g_Buttons[] =
{
  { kMenuCmdID_Toolbar_Add,     0, IDS_ADD,            true, false, true  },
  { kMenuCmdID_Toolbar_Extract, 1, IDS_EXTRACT,        true, false, true  },
  { kMenuCmdID_Toolbar_Test,    2, IDS_TEST,           true, false, true  },
  { IDM_FILE_VIEW,              3, IDS_BUTTON_VIEW,    true, false, true  },
  { IDM_DELETE,                 4, IDS_BUTTON_DELETE,  true, false, true  },
  { IDM_FIND_FILES,             5, IDS_BUTTON_FIND,    true, false, true  },
  { IDM_EASY_EXTRACT,           6, IDS_BUTTON_WIZARD,  true, false, true  },
  { IDM_PROPERTIES,             7, IDS_BUTTON_INFO,    true, false, true  },
  { IDM_COMMENT,                8, IDS_BUTTON_COMMENT, true, true,  true  },
  // new buttons must be added only at the end of table (saved layouts use indexes)
  { IDM_COPY_TO,                9, IDS_BUTTON_COPY,    true, false, false },
  { IDM_MOVE_TO,               10, IDS_BUTTON_MOVE,    true, false, false }
};

static const int kLayout_Separator = -1;

// default layout: all buttons, separator before "Comment"
static void GetDefaultToolbarLayout(CRecordVector<int> &layout)
{
  layout.Clear();
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Buttons); i++)
  {
    if (!g_Buttons[i].InDefault)
      continue;
    if (g_Buttons[i].SeparatorBefore)
      layout.Add(kLayout_Separator);
    layout.Add((int)i);
  }
}

static const wchar_t * const k_Reg_FM = L"Software\\7-Zip\\FM";
static const wchar_t * const k_Reg_ToolbarLayout = L"ToolbarLayout";

// returns false, if there is no saved layout
static bool ReadToolbarLayout(CRecordVector<int> &layout)
{
  layout.Clear();
  HKEY key;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, k_Reg_FM, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return false;
  wchar_t buf[1024];
  DWORD type = 0;
  DWORD size = (DWORD)(sizeof(buf) - sizeof(wchar_t));
  const LONG res = ::RegQueryValueExW(key, k_Reg_ToolbarLayout, NULL, &type, (LPBYTE)buf, &size);
  ::RegCloseKey(key);
  if (res != ERROR_SUCCESS || type != REG_SZ)
    return false;
  buf[size / sizeof(wchar_t)] = 0;

  bool used[Z7_ARRAY_SIZE(g_Buttons)];
  memset(used, 0, sizeof(used));
  const wchar_t *p = buf;
  for (;;)
  {
    while (*p == ' ' || *p == ',')
      p++;
    if (*p == 0)
      break;
    if (*p == '-')
    {
      layout.Add(kLayout_Separator);
      p++;
      continue;
    }
    if (*p < '0' || *p > '9')
    {
      p++;
      continue;
    }
    unsigned v = 0;
    while (*p >= '0' && *p <= '9' && v < 1000)
      v = v * 10 + (unsigned)(*p++ - '0');
    if (v < Z7_ARRAY_SIZE(g_Buttons) && !used[v])
    {
      used[v] = true;
      layout.Add((int)v);
    }
  }
  return true;
}

static const wchar_t * const k_Reg_ToolbarClassic = L"ToolbarClassicIcons";

bool ReadToolbarClassicIcons()
{
  HKEY key;
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, k_Reg_FM, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return false;
  DWORD value = 0;
  DWORD type = 0;
  DWORD size = sizeof(value);
  const LONG res = ::RegQueryValueExW(key, k_Reg_ToolbarClassic, NULL, &type, (LPBYTE)&value, &size);
  ::RegCloseKey(key);
  return (res == ERROR_SUCCESS && type == REG_DWORD && value != 0);
}

void SaveToolbarClassicIcons(bool classic)
{
  HKEY key;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, k_Reg_FM, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
    return;
  const DWORD value = classic ? 1 : 0;
  ::RegSetValueExW(key, k_Reg_ToolbarClassic, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
  ::RegCloseKey(key);
}

static void SaveToolbarLayout(const CRecordVector<int> &layout)
{
  UString s;
  FOR_VECTOR (i, layout)
  {
    if (i != 0)
      s += L',';
    if (layout[i] < 0)
      s += L'-';
    else
      s.Add_UInt32((UInt32)layout[i]);
  }
  HKEY key;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, k_Reg_FM, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
    return;
  ::RegSetValueExW(key, k_Reg_ToolbarLayout, 0, REG_SZ,
      (const BYTE *)(const wchar_t *)s, (DWORD)((s.Len() + 1) * sizeof(wchar_t)));
  ::RegCloseKey(key);
}

static int FindButtonByCommand(int commandID)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Buttons); i++)
    if (g_Buttons[i].CommandID == commandID)
      return (int)i;
  return -1;
}

static void SetButtonText(int commandID, UString &s)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Buttons); i++)
  {
    const CButtonInfo &b = g_Buttons[i];
    if (b.CommandID == commandID)
    {
      s = b.GetText();
      return;
    }
  }
}

static void AddSeparator(NControl::CToolBar &toolBar)
{
  TBBUTTON but;
  memset(&but, 0, sizeof(but));
  but.iBitmap = 8; // separator width
  but.idCommand = kMenuCmdID_Toolbar_Separator;
  but.fsState = TBSTATE_ENABLED;
  but.fsStyle = TBSTYLE_SEP;
  but.iString = -1;
  #ifdef _UNICODE
  toolBar.AddButton(1, &but);
  #else
  toolBar.AddButtonW(1, &but);
  #endif
}

static void AddButton(
    NControl::CToolBar &toolBar,
    const CButtonInfo &butInfo, bool showText)
{
  TBBUTTON but;
  memset(&but, 0, sizeof(but));
  but.iBitmap = (int)butInfo.ImageIndex;
  but.idCommand = butInfo.CommandID;
  but.fsState = TBSTATE_ENABLED;
  // BTNS_AUTOSIZE: width of each button depends on its own text,
  // so long text ("Comment") doesn't make all buttons wide.
  but.fsStyle = TBSTYLE_BUTTON | BTNS_AUTOSIZE;
  but.dwData = 0;

  UString s = butInfo.GetText();
  but.iString = -1;
  if (showText && butInfo.ShowText)
    but.iString = (INT_PTR)(LPCWSTR)s;

  #ifdef _UNICODE
  toolBar.AddButton(1, &but);
  #else
  toolBar.AddButtonW(1, &but);
  #endif
}

static LRESULT CALLBACK ToolbarSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR idSubclass, DWORD_PTR refData);

void CApp::ReloadToolbars()
{
  _buttonsImageList.Destroy();
  _logoImageList.Destroy();
  _chevronBar.Destroy();
  if (_chevronFont)
  {
    ::DeleteObject(_chevronFont);
    _chevronFont = NULL;
  }
  _toolBar.Destroy();
  _numHiddenToolbarCmds = 0;

  // there is only one toolbar now.
  if (ShowArchiveToolbar || ShowStandardToolbar)
  {
    _toolbarIconSize = CreateToolbar(_window, _buttonsImageList, _logoImageList, _toolBar, LargeButtons, ClassicIcons);
    if (_toolBar)
      ::SetWindowSubclass(_toolBar, ToolbarSubclassProc, 1, (DWORD_PTR)(HWND)_window);
    CRecordVector<int> layout;
    if (!ReadToolbarLayout(layout))
      GetDefaultToolbarLayout(layout);
    FillToolbarButtons(layout);
    _toolBar.AutoSize();
    CreateChevron();
  }
}

void CApp::FillToolbarButtons(const CRecordVector<int> &layout)
{
  if (!_toolBar)
    return;
  while (::SendMessage(_toolBar, TB_BUTTONCOUNT, 0, 0) > 0)
    if (!::SendMessage(_toolBar, TB_DELETEBUTTON, 0, 0))
      break;
  FOR_VECTOR (i, layout)
  {
    const int k = layout[i];
    if (k == kLayout_Separator)
      AddSeparator(_toolBar);
    else if (k >= 0 && (unsigned)k < Z7_ARRAY_SIZE(g_Buttons))
      AddButton(_toolBar, g_Buttons[(unsigned)k], ShowButtonsLables);
  }
}

// reads current buttons of toolbar
void CApp::GetCurrentToolbarLayout(CRecordVector<int> &layout)
{
  layout.Clear();
  if (!_toolBar)
    return;
  const int count = (int)::SendMessage(_toolBar, TB_BUTTONCOUNT, 0, 0);
  for (int i = 0; i < count; i++)
  {
    TBBUTTON b;
    if (!::SendMessage(_toolBar, TB_GETBUTTON, (WPARAM)i, (LPARAM)&b))
      continue;
    if (b.fsStyle & TBSTYLE_SEP)
    {
      // no separator at start, and no 2 separators together
      if (!layout.IsEmpty() && layout.Back() != kLayout_Separator)
        layout.Add(kLayout_Separator);
      continue;
    }
    const int k = FindButtonByCommand(b.idCommand);
    if (k >= 0)
      layout.Add(k);
  }
  if (!layout.IsEmpty() && layout.Back() == kLayout_Separator)
    layout.DeleteBack();
}

// reads current buttons of toolbar and saves them to registry
void CApp::SaveCurrentToolbarLayout()
{
  if (!_toolBar)
    return;
  CRecordVector<int> layout;
  GetCurrentToolbarLayout(layout);
  SaveToolbarLayout(layout);
}

static UString GetMainMenuText(HWND wnd, UINT id)
{
  wchar_t buf[256];
  buf[0] = 0;
  const HMENU menu = ::GetMenu(wnd);
  if (menu)
    ::GetMenuStringW(menu, id, buf, (int)Z7_ARRAY_SIZE(buf), MF_BYCOMMAND);
  return UString(buf);
}

/*
  Right click on toolbar:
    Large Buttons, Show Buttons Text, Customize..., Reset
  (buttons are selected in "Customize Toolbar" dialog)
*/
void CApp::ShowToolbarContextMenu()
{
  if (!_toolBar)
    return;
  const unsigned kCmd_Large = 100;
  const unsigned kCmd_Text = 101;
  const unsigned kCmd_Customize = 102;
  const unsigned kCmd_Reset = 103;
  const unsigned kCmd_Classic = 104;

  HMENU menu = ::CreatePopupMenu();
  if (!menu)
    return;
  {
    UString s = GetMainMenuText(_window, IDM_VIEW_TOOLBARS_LARGE_BUTTONS);
    if (s.IsEmpty())
      s = "Large Buttons";
    ::AppendMenuW(menu, MF_STRING | (LargeButtons ? MF_CHECKED : MF_UNCHECKED), kCmd_Large, s);
  }
  {
    UString s = GetMainMenuText(_window, IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT);
    if (s.IsEmpty())
      s = "Show Buttons Text";
    ::AppendMenuW(menu, MF_STRING | (ShowButtonsLables ? MF_CHECKED : MF_UNCHECKED), kCmd_Text, s);
  }
  {
    UString s = GetMainMenuText(_window, IDM_VIEW_TOOLBARS_CLASSIC);
    if (s.IsEmpty())
      s = "Classic Icons";
    ::AppendMenuW(menu, MF_STRING | (ClassicIcons ? MF_CHECKED : MF_UNCHECKED), kCmd_Classic, s);
  }
  ::AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
  {
    UString s = GetMainMenuText(_window, IDM_VIEW_TOOLBARS_CUSTOMIZE);
    if (s.IsEmpty())
      s = "&Customize...";
    ::AppendMenuW(menu, MF_STRING, kCmd_Customize, s);
  }
  ::AppendMenuW(menu, MF_STRING, kCmd_Reset, L"&Reset");

  POINT pt;
  ::GetCursorPos(&pt);
  const unsigned cmd = (unsigned)::TrackPopupMenu(menu,
      TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
      pt.x, pt.y, 0, _window, NULL);
  ::DestroyMenu(menu);
  if (cmd == 0)
    return;

  // we are inside notification from toolbar, so we can't destroy toolbar here.
  // So we post commands, and toolbar is rebuilt later.
  if (cmd == kCmd_Large)
    ::PostMessage(_window, WM_COMMAND, IDM_VIEW_TOOLBARS_LARGE_BUTTONS, 0);
  else if (cmd == kCmd_Text)
    ::PostMessage(_window, WM_COMMAND, IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT, 0);
  else if (cmd == kCmd_Customize)
    ::PostMessage(_window, WM_COMMAND, IDM_VIEW_TOOLBARS_CUSTOMIZE, 0);
  else if (cmd == kCmd_Classic)
    ::PostMessage(_window, WM_COMMAND, IDM_VIEW_TOOLBARS_CLASSIC, 0);
  else if (cmd == kCmd_Reset)
  {
    CRecordVector<int> layout;
    GetDefaultToolbarLayout(layout);
    SaveToolbarLayout(layout);
    ::PostMessage(_window, WM_COMMAND, kMenuCmdID_Toolbar_Reload, 0);
  }
}

// small icons for lists in "Customize Toolbar" window
static HIMAGELIST CreateSmallToolbarImageList(bool classic)
{
  int dpi = 96;
  const HDC dc = ::GetDC(NULL);
  if (dc)
  {
    dpi = ::GetDeviceCaps(dc, LOGPIXELSY);
    ::ReleaseDC(NULL, dc);
  }
  if (dpi <= 0)
    dpi = 96;
  unsigned size = 24;
  UINT resID = IDB_TBC_S; // classic small icons: 24x24
  if (!classic)
  {
    const unsigned want = (unsigned)(20 * dpi / 96);
    static const unsigned kSizes[] = { 16, 20, 24, 32, 40, 48 };
    static const UINT kIDs[] = { IDB_TB16, IDB_TB20, IDB_TB24, IDB_TB32, IDB_TB40, IDB_TB48 };
    unsigned best = 0;
    for (unsigned k = 0; k < Z7_ARRAY_SIZE(kSizes); k++)
      if (kSizes[k] <= want)
        best = k;
    size = kSizes[best];
    resID = kIDs[best];
  }
  HIMAGELIST il = ImageList_Create((int)size, (int)size, ILC_COLOR32, (int)kNumToolbarImages, 0);
  if (!il)
    return NULL;
  HBITMAP b = LoadAlphaBitmap(resID);
  if (b)
  {
    ImageList_Add(il, b, NULL);
    ::DeleteObject(b);
  }
  return il;
}

// our own "Customize Toolbar" window (compact lists with small icons)
void CApp::CustomizeToolbar()
{
  if (!_toolBar)
  {
    ShowArchiveToolbar = true;
    SaveToolbarChanges();
  }
  CToolbarCustomizeDialog dlg;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Buttons); i++)
  {
    CToolbarButtonDesc &d = dlg.Buttons.AddNew();
    d.Text = g_Buttons[i].GetText();
    d.ImageIndex = (int)g_Buttons[i].ImageIndex;
  }
  GetCurrentToolbarLayout(dlg.Layout);
  GetDefaultToolbarLayout(dlg.DefaultLayout);
  dlg.ImageList = CreateSmallToolbarImageList(ClassicIcons);
  const INT_PTR res = dlg.Create(_window);
  if (dlg.ImageList)
    ImageList_Destroy(dlg.ImageList);
  if (res != IDOK)
    return;
  SaveToolbarLayout(dlg.Layout);
  SaveToolbarChanges(); // rebuild toolbar
}

// double click on empty part of toolbar: our "Customize Toolbar" window
// (instead of Windows dialog of CCS_ADJUSTABLE toolbar)
static LRESULT CALLBACK ToolbarSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
    UINT_PTR /* idSubclass */, DWORD_PTR refData)
{
  if (message == WM_LBUTTONDBLCLK)
  {
    POINT pt;
    pt.x = (short)LOWORD(lParam);
    pt.y = (short)HIWORD(lParam);
    const int hit = (int)::SendMessage(hwnd, TB_HITTEST, 0, (LPARAM)&pt);
    if (hit < 0)
    {
      ::PostMessage((HWND)refData, WM_COMMAND, IDM_VIEW_TOOLBARS_CUSTOMIZE, 0);
      return 0;
    }
  }
  else if (message == WM_NCDESTROY)
    ::RemoveWindowSubclass(hwnd, ToolbarSubclassProc, 1);
  return ::DefSubclassProc(hwnd, message, wParam, lParam);
}

// toolbar notifications for "Customize Toolbar" dialog and Alt+drag
bool CApp::OnToolbarAdjustNotify(LPNMHDR pnmh, LRESULT &result)
{
  result = 0;
  switch (pnmh->code)
  {
    case TBN_QUERYINSERT:
    case TBN_QUERYDELETE:
      result = TRUE;
      return true;

    case NM_RCLICK:
      ShowToolbarContextMenu();
      result = TRUE; // we processed it
      return true;

    case TBN_GETBUTTONINFOW:
    {
      // list of available buttons in dialog
      LPNMTOOLBARW tb = (LPNMTOOLBARW)pnmh;
      if (tb->iItem < 0 || (unsigned)tb->iItem >= Z7_ARRAY_SIZE(g_Buttons))
      {
        result = FALSE;
        return true;
      }
      const CButtonInfo &b = g_Buttons[(unsigned)tb->iItem];
      memset(&tb->tbButton, 0, sizeof(tb->tbButton));
      tb->tbButton.iBitmap = (int)b.ImageIndex;
      tb->tbButton.idCommand = b.CommandID;
      tb->tbButton.fsState = TBSTATE_ENABLED;
      tb->tbButton.fsStyle = TBSTYLE_BUTTON | BTNS_AUTOSIZE;
      tb->tbButton.iString = -1;
      if (tb->pszText && tb->cchText > 0)
      {
        const UString s = b.GetText();
        ::lstrcpynW(tb->pszText, s, tb->cchText);
      }
      result = TRUE;
      return true;
    }

    case TBN_BEGINADJUST:
      _toolbarAdjusting = true;
      UpdateToolbarOverflow(0x7FFFFFFF); // show all buttons, hide chevron
      return true;

    case TBN_RESET:
    {
      // "Reset" button in dialog: default buttons
      CRecordVector<int> layout;
      GetDefaultToolbarLayout(layout);
      FillToolbarButtons(layout);
      return true;
    }

    case TBN_TOOLBARCHANGE:
      if (!_toolbarAdjusting)
      {
        // Alt+drag of button
        SaveCurrentToolbarLayout();
        ::PostMessage(_window, WM_COMMAND, kMenuCmdID_Toolbar_Reload, 0);
      }
      return true;

    case TBN_ENDADJUST:
      _toolbarAdjusting = false;
      SaveCurrentToolbarLayout();
      // we can't destroy toolbar inside its own notification. So we reload it later.
      ::PostMessage(_window, WM_COMMAND, kMenuCmdID_Toolbar_Reload, 0);
      return true;

    default:
      break;
  }
  return false;
}


/*
  ">>" (chevron) button:
  if the window is too narrow for all toolbar buttons, the buttons that don't fit
  are hidden, and small ">>" button is shown at the right end of toolbar.
  Click on it shows popup menu with hidden buttons.
  The chevron is separate small toolbar window over the right end of main toolbar.
*/

void CApp::CreateChevron()
{
  _chevronBar.Attach(::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 0
      | WS_CHILD
      | WS_CLIPSIBLINGS
      | TBSTYLE_FLAT
      | TBSTYLE_LIST
      | CCS_NORESIZE
      | CCS_NOPARENTALIGN
      | CCS_NODIVIDER
      ,0,0,0,0, _window, NULL, g_hInstance, NULL));
  if (!_chevronBar)
    return;
  _chevronBar.ButtonStructSize();
  ::SendMessage(_chevronBar, TB_SETBITMAPSIZE, 0, MAKELPARAM(1, 1));

  // big bold font, so the chevron is easy to see
  _chevronFont = ::CreateFontW(-(int)(_toolbarIconSize * 2 / 3), 0, 0, 0, FW_BOLD,
      FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  if (_chevronFont)
    ::SendMessage(_chevronBar, WM_SETFONT, (WPARAM)_chevronFont, FALSE);

  TBBUTTON but;
  memset(&but, 0, sizeof(but));
  but.iBitmap = I_IMAGENONE;
  but.idCommand = kMenuCmdID_Toolbar_Chevron;
  but.fsState = TBSTATE_ENABLED;
  but.fsStyle = TBSTYLE_BUTTON;
  but.iString = (INT_PTR)(LPCWSTR)L"\x00BB";
  #ifdef _UNICODE
  _chevronBar.AddButton(1, &but);
  #else
  _chevronBar.AddButtonW(1, &but);
  #endif
}

// by index: separators can have same ID
static void Toolbar_ShowButton_ByIndex(HWND tb, int index, bool show)
{
  TBBUTTONINFOW bi;
  memset(&bi, 0, sizeof(bi));
  bi.cbSize = sizeof(bi);
  bi.dwMask = TBIF_BYINDEX | TBIF_STATE;
  if (::SendMessage(tb, TB_GETBUTTONINFOW, (WPARAM)index, (LPARAM)&bi) < 0)
    return;
  BYTE state = bi.fsState;
  if (show)
    state = (BYTE)(state & ~TBSTATE_HIDDEN);
  else
    state = (BYTE)(state | TBSTATE_HIDDEN);
  if (state == bi.fsState)
    return;
  bi.fsState = state;
  ::SendMessage(tb, TB_SETBUTTONINFOW, (WPARAM)index, (LPARAM)&bi);
}

void CApp::UpdateToolbarOverflow(int width)
{
  _numHiddenToolbarCmds = 0;
  if (!_toolBar)
    return;
  HWND tb = _toolBar;
  int count = (int)::SendMessage(tb, TB_BUTTONCOUNT, 0, 0);

  int i;
  for (i = 0; i < count; i++)
    Toolbar_ShowButton_ByIndex(tb, i, true);

  bool overflow = false;
  if (_toolbarAdjusting)
    count = 0; // "Customize Toolbar" dialog is open: show all buttons
  if (count > 0)
  {
    RECT r;
    if (::SendMessage(tb, TB_GETITEMRECT, (WPARAM)(count - 1), (LPARAM)&r))
      overflow = (r.right > width);
  }

  if (!overflow)
  {
    if (_chevronBar)
      _chevronBar.Show(SW_HIDE);
    return;
  }

  const int chevronWidth = (int)(_toolbarIconSize * 2 / 3) + 12;
  const int avail = width - chevronWidth;

  int first = count;
  for (i = 0; i < count; i++)
  {
    RECT r;
    if (::SendMessage(tb, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&r) && r.right > avail)
    {
      first = i;
      break;
    }
  }
  // don't leave a separator at the end of visible buttons
  while (first > 0)
  {
    TBBUTTON b;
    if (!::SendMessage(tb, TB_GETBUTTON, (WPARAM)(first - 1), (LPARAM)&b))
      break;
    if ((b.fsStyle & TBSTYLE_SEP) == 0)
      break;
    first--;
  }

  for (i = first; i < count; i++)
  {
    TBBUTTON b;
    if (!::SendMessage(tb, TB_GETBUTTON, (WPARAM)i, (LPARAM)&b))
      continue;
    if ((b.fsStyle & TBSTYLE_SEP) == 0
        && _numHiddenToolbarCmds < Z7_ARRAY_SIZE(_hiddenToolbarCmds))
      _hiddenToolbarCmds[_numHiddenToolbarCmds++] = b.idCommand;
  }
  // hide from the end, so indexes of other buttons are not changed
  for (i = count - 1; i >= first; i--)
    Toolbar_ShowButton_ByIndex(tb, i, false);

  if (!_chevronBar)
    return;
  RECT tr;
  ::GetWindowRect(tb, &tr);
  const int h = (int)(tr.bottom - tr.top);
  POINT pt;
  pt.x = tr.left;
  pt.y = tr.top;
  ::ScreenToClient(_window, &pt);
  ::SendMessage(_chevronBar, TB_SETBUTTONSIZE, 0, MAKELPARAM(chevronWidth, h > 4 ? h - 4 : h));
  ::SetWindowPos(_chevronBar, HWND_TOP, width - chevronWidth, pt.y + 2, chevronWidth, h > 4 ? h - 4 : h,
      SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

static int GetButtonImageIndex(int commandID)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Buttons); i++)
    if (g_Buttons[i].CommandID == commandID)
      return (int)g_Buttons[i].ImageIndex;
  return -1;
}

// small icon for menu item:
// 32-bit top-down DIB with premultiplied alpha (Vista+ menus draw it with alpha).
// (strip) is 32-bit DIB section loaded with LR_CREATEDIBSECTION.
static HBITMAP CreateMenuIcon(HBITMAP strip, int imageIndex)
{
  DIBSECTION ds;
  if (::GetObject(strip, sizeof(ds), &ds) != sizeof(ds))
    return NULL;
  if (ds.dsBm.bmBitsPixel != 32 || !ds.dsBm.bmBits)
    return NULL;
  const int size = ds.dsBm.bmHeight;
  if (size <= 0 || (imageIndex + 1) * size > ds.dsBm.bmWidth)
    return NULL;
  const bool bottomUp = (ds.dsBmih.biHeight > 0);

  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = size;
  bi.bmiHeader.biHeight = -size; // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *bits = NULL;
  HBITMAP bmp = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  if (!bmp || !bits)
    return bmp;

  Byte *dest = (Byte *)bits;
  for (int y = 0; y < size; y++)
  {
    const int srcRow = bottomUp ? (size - 1 - y) : y;
    const Byte *src = (const Byte *)ds.dsBm.bmBits
        + (size_t)srcRow * (size_t)ds.dsBm.bmWidthBytes
        + (size_t)imageIndex * (size_t)size * 4;
    for (int x = 0; x < size; x++)
    {
      const unsigned a = src[3];
      dest[0] = (Byte)((src[0] * a + 127) / 255);
      dest[1] = (Byte)((src[1] * a + 127) / 255);
      dest[2] = (Byte)((src[2] * a + 127) / 255);
      dest[3] = (Byte)a;
      src += 4;
      dest += 4;
    }
  }
  return bmp;
}

void CApp::ShowToolbarOverflowMenu()
{
  if (_numHiddenToolbarCmds == 0 || !_chevronBar)
    return;
  HMENU menu = ::CreatePopupMenu();
  if (!menu)
    return;

  // small icon strip with size closest to system small icon size
  HBITMAP strip = NULL;
  {
    const int want = ::GetSystemMetrics(SM_CXSMICON);
    static const int kSizes[] = { 16, 20, 24, 32 };
    static const UINT kIDs[] = { IDB_TB16, IDB_TB20, IDB_TB24, IDB_TB32 };
    unsigned best = 0;
    for (unsigned k = 1; k < Z7_ARRAY_SIZE(kSizes); k++)
    {
      const int d1 = kSizes[k] - want;
      const int d0 = kSizes[best] - want;
      if ((d1 < 0 ? -d1 : d1) < (d0 < 0 ? -d0 : d0))
        best = k;
    }
    // classic icons: small 24x24 classic strip
    strip = LoadAlphaBitmap(ClassicIcons ? (UINT)IDB_TBC_S : kIDs[best]);
  }

  HBITMAP icons[32]; // same size as _hiddenToolbarCmds[]
  unsigned numIcons = 0;

  for (unsigned i = 0; i < _numHiddenToolbarCmds; i++)
  {
    const int id = _hiddenToolbarCmds[i];
    UString s;
    SetButtonText(id, s);

    MENUITEMINFOW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE;
    mi.fType = MFT_STRING;
    mi.wID = (UINT)id;
    mi.dwTypeData = s.Ptr_non_const();
    mi.cch = s.Len();

    HBITMAP icon = NULL;
    const int imageIndex = GetButtonImageIndex(id);
    if (strip && imageIndex >= 0)
      icon = CreateMenuIcon(strip, imageIndex);
    if (icon)
    {
      mi.fMask |= MIIM_BITMAP;
      mi.hbmpItem = icon;
      icons[numIcons++] = icon;
    }
    ::InsertMenuItemW(menu, i, TRUE, &mi);
  }
  if (strip)
    ::DeleteObject(strip);
  RECT r;
  ::GetWindowRect(_chevronBar, &r);
  const int cmd = (int)::TrackPopupMenu(menu,
      TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_NONOTIFY,
      r.right, r.bottom, 0, _window, NULL);
  ::DestroyMenu(menu);
  for (unsigned k = 0; k < numIcons; k++)
    ::DeleteObject(icons[k]);
  if (cmd != 0)
    ::PostMessage(_window, WM_COMMAND, MAKEWPARAM((unsigned)cmd, 0), 0);
}

void CApp::SaveToolbarChanges()
{
  SaveToolbar();
  ReloadToolbars();
  MoveSubWindows();
}


HRESULT CApp::Create(HWND hwnd, const UString &mainPath, const UString &arcFormat, int xSizes[2], bool needOpenArc, COpenResult &openRes)
{
  _window.Attach(hwnd);
  NDarkMode::ApplyToMainWindow(_window);

  #ifdef UNDER_CE
  _commandBar.Create(g_hInstance, hwnd, 1);
  #endif

  MyLoadMenu(false);  // needResetMenu
  
  #ifdef UNDER_CE
  _commandBar.AutoSize();
  #endif

  ReadToolbar();
  ReloadToolbars();

  unsigned i;
  for (i = 0; i < kNumPanelsMax; i++)
    Panels[i].PanelCreated = false;

  AppState.Read();
  
  SetListSettings();

  if (LastFocusedPanel >= kNumPanelsMax)
    LastFocusedPanel = 0;
  // ShowDeletedFiles = Read_ShowDeleted();

  CListMode listMode;
  listMode.Read();
  
  for (i = 0; i < kNumPanelsMax; i++)
  {
    CPanel &panel = Panels[i];
    panel._listViewMode = listMode.Panels[i];
    panel._xSize = xSizes[i];
    panel._flatModeForArc = ReadFlatView(i);
  }
  
  for (i = 0; i < kNumPanelsMax; i++)
  {
    unsigned panelIndex = i;
    if (needOpenArc && LastFocusedPanel == 1)
      panelIndex = 1 - i;

    bool isMainPanel = (panelIndex == LastFocusedPanel);

    if (NumPanels > 1 || isMainPanel)
    {
      if (NumPanels == 1)
        Panels[panelIndex]._xSize = xSizes[0] + xSizes[1];
      
      COpenResult openRes2;
      UString path;
      if (isMainPanel)
        path = mainPath;
      
      RINOK(CreateOnePanel(panelIndex, path, arcFormat,
          isMainPanel && needOpenArc,
          *(isMainPanel ? &openRes : &openRes2)))
      
      if (isMainPanel)
      {
        if (needOpenArc && !openRes.ArchiveIsOpened)
          return S_OK;
      }
    }
  }
  
  SetFocusedPanel(LastFocusedPanel);
  Panels[LastFocusedPanel].SetFocusToList();
  return S_OK;
}


HRESULT CApp::SwitchOnOffOnePanel()
{
  if (NumPanels == 1)
  {
    NumPanels++;
    COpenResult openRes;
    RINOK(CreateOnePanel(1 - LastFocusedPanel, UString(), UString(),
        false, // needOpenArc
        openRes))
    Panels[1 - LastFocusedPanel].Enable(true);
    Panels[1 - LastFocusedPanel].Show(SW_SHOWNORMAL);
  }
  else
  {
    NumPanels--;
    Panels[1 - LastFocusedPanel].Enable(false);
    Panels[1 - LastFocusedPanel].Show(SW_HIDE);
  }
  MoveSubWindows();
  return S_OK;
}

void CApp::Save()
{
  AppState.Save();
  CListMode listMode;
  
  for (unsigned i = 0; i < kNumPanelsMax; i++)
  {
    const CPanel &panel = Panels[i];
    UString path;
    if (panel._parentFolders.IsEmpty())
      path = panel._currentFolderPrefix;
    else
      path = panel._parentFolders[0].ParentFolderPath;
      // GetFolderPath(panel._parentFolders[0].ParentFolder);
    SavePanelPath(i, path);
    listMode.Panels[i] = panel.GetListViewMode();
    SaveFlatView(i, panel._flatModeForArc);
  }
  
  listMode.Save();
  // Save_ShowDeleted(ShowDeletedFiles);
}

void CApp::ReleaseApp()
{
  // 24.09: ReleasePanel() will stop panel timer processing.
  // but we want to stop timer processing for all panels
  // before ReleasePanel() calling.
  unsigned i;
  for (i = 0; i < kNumPanelsMax; i++)
    Panels[i].Disable_Processing_Timer_Notify_StatusBar();
  // It's for unloading COM dll's: don't change it.
  for (i = 0; i < kNumPanelsMax; i++)
    Panels[i].ReleasePanel();
}

// reduces path to part that exists on disk (or root prefix of path)
// output path is normalized (with WCHAR_PATH_SEPARATOR)
static void Reduce_Path_To_RealFileSystemPath(UString &path)
{
  unsigned prefixSize = GetRootPrefixSize(path);

  while (!path.IsEmpty())
  {
    if (NFind::DoesDirExist_FollowLink(us2fs(path)))
    {
      NName::NormalizeDirPathPrefix(path);
      break;
    }
    int pos = path.ReverseFind_PathSepar();
    if (pos < 0)
    {
      path.Empty();
      break;
    }
    path.DeleteFrom((unsigned)(pos + 1));
    if ((unsigned)pos + 1 == prefixSize)
      break;
    path.DeleteFrom((unsigned)pos);
  }
}

// returns: true, if such dir exists or is root
/*
static bool CheckFolderPath(const UString &path)
{
  UString pathReduced = path;
  Reduce_Path_To_RealFileSystemPath(pathReduced);
  return (pathReduced == path);
}
*/

extern UString ConvertSizeToString(UInt64 value);

static void AddSizeValue(UString &s, UInt64 size)
{
  s += MyFormatNew(IDS_FILE_SIZE, ConvertSizeToString(size));
}

static void AddValuePair1(UString &s, UINT resourceID, UInt64 size)
{
  AddLangString(s, resourceID);
  s += ": ";
  AddSizeValue(s, size);
  s.Add_LF();
}

void AddValuePair2(UString &s, UINT resourceID, UInt64 num, UInt64 size);
void AddValuePair2(UString &s, UINT resourceID, UInt64 num, UInt64 size)
{
  if (num == 0)
    return;
  AddLangString(s, resourceID);
  s += ": ";
  s += ConvertSizeToString(num);

  if (size != (UInt64)(Int64)-1)
  {
    s += "    ( ";
    AddSizeValue(s, size);
    s += " )";
  }
  s.Add_LF();
}

static void AddPropValueToSum(IFolderFolder *folder, UInt32 index, PROPID propID, UInt64 &sum)
{
  if (sum == (UInt64)(Int64)-1)
    return;
  NCOM::CPropVariant prop;
  folder->GetProperty(index, propID, &prop);
  UInt64 val = 0;
  if (ConvertPropVariantToUInt64(prop, val))
    sum += val;
  else
    sum = (UInt64)(Int64)-1;
}

UString CPanel::GetItemsInfoString(const CRecordVector<UInt32> &indices)
{
  UString info;
  UInt64 numDirs, numFiles, filesSize, foldersSize;
  numDirs = numFiles = filesSize = foldersSize = 0;
  
  unsigned i;
  for (i = 0; i < indices.Size(); i++)
  {
    const UInt32 index = indices[i];
    if (IsItem_Folder(index))
    {
      AddPropValueToSum(_folder, index, kpidSize, foldersSize);
      numDirs++;
    }
    else
    {
      AddPropValueToSum(_folder, index, kpidSize, filesSize);
      numFiles++;
    }
  }

  AddValuePair2(info, IDS_PROP_FOLDERS, numDirs, foldersSize);
  AddValuePair2(info, IDS_PROP_FILES, numFiles, filesSize);
  int numDefined = ((foldersSize != (UInt64)(Int64)-1) && foldersSize != 0) ? 1: 0;
  numDefined += ((filesSize != (UInt64)(Int64)-1) && filesSize != 0) ? 1: 0;
  if (numDefined == 2)
    AddValuePair1(info, IDS_PROP_SIZE, filesSize + foldersSize);
  
  info.Add_LF();
  info += _currentFolderPrefix;
  
  for (i = 0; i < indices.Size() && (int)i < (int)kCopyDialog_NumInfoLines - 6; i++)
  {
    info.Add_LF();
    info += "  ";
    const UInt32 index = indices[i];
    info += GetItemRelPath(index);
    if (IsItem_Folder(index))
      info.Add_PathSepar();
  }
  if (i != indices.Size())
  {
    info.Add_LF();
    info += "  ...";
  }
  return info;
}

bool IsCorrectFsName(const UString &name);



/* Returns true, if path is path that can be used as path for File System functions
*/

/*
static bool IsFsPath(const FString &path)
{
  if (!IsAbsolutePath(path))
    return false;
  unsigned prefixSize = GetRootPrefixSize(path);
}
*/

// function from ContextMenu.cpp: archive name without extension
UString GetSubFolderNameForExtract(const UString &arcName);

static bool g_EasyExtract_OpenAfter = true;

// opens the folder in Windows File Explorer
void OpenFolderInExplorer(const UString &folder);
void OpenFolderInExplorer(const UString &folder)
{
  if (folder.IsEmpty())
    return;
  ::ShellExecuteW(NULL, L"open", folder, NULL, NULL, SW_SHOWNORMAL);
}

void CApp::OnCopy(bool move, bool copyToSame, unsigned srcPanelIndex, bool easyExtract)
{
  bool openAfter = false;
  const unsigned destPanelIndex = (NumPanels <= 1) ? srcPanelIndex : (1 - srcPanelIndex);
  CPanel &srcPanel = Panels[srcPanelIndex];
  CPanel &destPanel = Panels[destPanelIndex];

  CPanel::CDisableTimerProcessing disableTimerProcessing1(destPanel);
  CPanel::CDisableTimerProcessing disableTimerProcessing2(srcPanel);

  if (move)
  {
    if (!srcPanel.CheckBeforeUpdate(IDS_MOVE))
      return;
  }
  else if (!srcPanel.DoesItSupportOperations())
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }

  CRecordVector<UInt32> indices;
  UString destPath;
  bool useDestPanel = false;

  {
    if (copyToSame)
    {
      const int focusedItem = srcPanel._listView.GetFocusedItem();
      if (focusedItem < 0)
        return;
      const unsigned realIndex = srcPanel.GetRealItemIndex(focusedItem);
      if (realIndex == kParentIndex)
        return;
      indices.Add(realIndex);
      destPath = srcPanel.GetItemName(realIndex);
    }
    else
    {
      srcPanel.Get_ItemIndices_OperSmart(indices);
      if (indices.Size() == 0)
        return;
      destPath = destPanel.GetFsPath();
      if (NumPanels == 1)
        Reduce_Path_To_RealFileSystemPath(destPath);
      if (easyExtract && !srcPanel._parentFolders.IsEmpty())
      {
        // add folder with archive name (without extension)
        const UString &arcPath = srcPanel._parentFolders[0].VirtualPath;
        const int slash = arcPath.ReverseFind_PathSepar();
        const UString arcName (arcPath.Ptr((unsigned)(slash + 1)));
        const UString sub = GetSubFolderNameForExtract(arcName);
        if (!sub.IsEmpty())
        {
          if (!destPath.IsEmpty() && !IS_PATH_SEPAR(destPath.Back()))
            destPath.Add_PathSepar();
          destPath += sub;
          destPath.Add_PathSepar();
        }
      }
    }
  }
  
  UStringVector copyFolders;
  ReadCopyHistory(copyFolders);
  
  const bool useFullItemPaths = srcPanel.Is_IO_FS_Folder(); // maybe we need flat also here ??

  {
    CCopyDialog copyDialog;

    copyDialog.Strings = copyFolders;
    copyDialog.Value = destPath;
    LangString(move ? IDS_MOVE : IDS_COPY, copyDialog.Title);
    LangString(move ? IDS_MOVE_TO : IDS_COPY_TO, copyDialog.Static);
    copyDialog.Info = srcPanel.GetItemsInfoString(indices);
    if (easyExtract)
    {
      // info text: archive name without extension, like in "Extract to" path
      if (!srcPanel._parentFolders.IsEmpty())
      {
        const UString &arcPath = srcPanel._parentFolders[0].VirtualPath;
        if (copyDialog.Info.IsPrefixedBy(arcPath))
        {
          const int slash = arcPath.ReverseFind_PathSepar();
          UString s = arcPath;
          s.DeleteFrom((unsigned)(slash + 1));
          s += GetSubFolderNameForExtract(UString(arcPath.Ptr((unsigned)(slash + 1))));
          s += copyDialog.Info.Ptr(arcPath.Len());
          copyDialog.Info = s;
        }
      }
      LangString(IDS_EASY_EXTRACT, copyDialog.Title);
      LangString(IDS_EASY_EXTRACT_TO, copyDialog.Static);
      LangString(IDS_EASY_EXTRACT_OPEN, copyDialog.OpenOptionText);
      copyDialog.ShowOpenOption = true;
      copyDialog.IconResID = IDI_EASY_EXTRACT;
      copyDialog.OpenAfter = g_EasyExtract_OpenAfter;
    }

    if (copyDialog.Create(srcPanel.GetParent()) != IDOK)
      return;

    destPath = copyDialog.Value;
    if (easyExtract)
    {
      g_EasyExtract_OpenAfter = copyDialog.OpenAfter;
      openAfter = copyDialog.OpenAfter;
    }
  }

  {
    if (destPath.IsEmpty())
    {
      srcPanel.MessageBox_Error_UnsupportOperation();
      return;
    }

    UString correctName;
    if (!srcPanel.CorrectFsPath(destPath, correctName))
    {
      srcPanel.MessageBox_Error_HRESULT(E_INVALIDARG);
      return;
    }

    if (IsAbsolutePath(destPath))
      destPath.Empty();
    else
      destPath = srcPanel.GetFsPath();
    destPath += correctName;

    #if defined(_WIN32) && !defined(UNDER_CE)
    if (destPath.Len() != 0 && destPath[0] == '\\')
      if (destPath.Len() == 1 || destPath[1] != '\\')
      {
        srcPanel.MessageBox_Error_UnsupportOperation();
        return;
      }
    #endif

    bool possibleToUseDestPanel = false;

    if (CompareFileNames(destPath, destPanel.GetFsPath()) == 0)
    {
      if (NumPanels == 1 || CompareFileNames(destPath, srcPanel.GetFsPath()) == 0)
      {
        srcPanel.MessageBox_Error(L"Cannot copy files onto itself");
        return;
      }

      if (destPanel.DoesItSupportOperations())
        possibleToUseDestPanel = true;
    }

    bool destIsFsPath = false;

    if (possibleToUseDestPanel)
    {
      if (destPanel.IsFSFolder() || destPanel.IsAltStreamsFolder())
        destIsFsPath = true;
      else if (destPanel.IsFSDrivesFolder() || destPanel.IsRootFolder())
      {
        srcPanel.MessageBox_Error_UnsupportOperation();
        return;
      }
    }
    else
    {
      if (IsAltPathPrefix(us2fs(destPath)))
      {
        // we allow alt streams dest only to alt stream folder in second panel
        srcPanel.MessageBox_Error_UnsupportOperation();
        return;
        /*
        FString basePath = us2fs(destPath);
        basePath.DeleteBack();
        if (!DoesFileOrDirExist(basePath))
        {
          srcPanel.MessageBoxError2Lines(basePath, ERROR_FILE_NOT_FOUND); // GetLastError()
          return;
        }
        destIsFsPath = true;
        */
      }
      else
      {
        if (indices.Size() == 1 &&
          !destPath.IsEmpty() && !IS_PATH_SEPAR(destPath.Back()))
        {
          int pos = destPath.ReverseFind_PathSepar();
          if (pos < 0)
          {
            srcPanel.MessageBox_Error_UnsupportOperation();
            return;
          }
          {
            /*
            #ifdef _WIN32
            UString name = destPath.Ptr(pos + 1);
            if (name.Find(L':') >= 0)
            {
              srcPanel.MessageBox_Error_UnsupportOperation();
              return;
            }
            #endif
            */
            UString prefix = destPath.Left(pos + 1);
            if (!CreateComplexDir(us2fs(prefix)))
            {
              const HRESULT lastError = GetLastError_noZero_HRESULT();
              srcPanel.MessageBox_Error_2Lines_Message_HRESULT(prefix, lastError);
              return;
            }
          }
          // bool isFolder = srcPanael.IsItem_Folder(indices[0]);
        }
        else
        {
          NName::NormalizeDirPathPrefix(destPath);
          if (!CreateComplexDir(us2fs(destPath)))
          {
            const HRESULT lastError = GetLastError_noZero_HRESULT();
            srcPanel.MessageBox_Error_2Lines_Message_HRESULT(destPath, lastError);
            return;
          }
        }
        destIsFsPath = true;
      }
    }

    if (!destIsFsPath)
      useDestPanel = true;

    AddUniqueStringToHeadOfList(copyFolders, destPath);
    while (copyFolders.Size() > 20)
      copyFolders.DeleteBack();
    SaveCopyHistory(copyFolders);
  }

  bool useSrcPanel = !useDestPanel || !srcPanel.Is_IO_FS_Folder();

  bool useTemp = useSrcPanel && useDestPanel;
  if (useTemp && NumPanels == 1)
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }
  
  CTempDir tempDirectory;
  FString tempDirPrefix;
  if (useTemp)
  {
    tempDirectory.Create(kTempDirPrefix);
    tempDirPrefix = tempDirectory.GetPath();
    NFile::NName::NormalizeDirPathPrefix(tempDirPrefix);
  }

  CSelectedState srcSelState;
  CSelectedState destSelState;
  srcPanel.SaveSelectedState(srcSelState);
  destPanel.SaveSelectedState(destSelState);

  CPanel::CDisableNotify disableNotify1(destPanel);
  CPanel::CDisableNotify disableNotify2(srcPanel);

  HRESULT result = S_OK;
  
  if (useSrcPanel)
  {
    CCopyToOptions options;
    // options.src_Is_IO_FS_Folder = useFullItemPaths;
    options.folder = useTemp ? fs2us(tempDirPrefix) : destPath;
    options.moveMode = move;
    options.includeAltStreams = true;
    options.replaceAltStreamChars = false;
    options.showErrorMessages = true;

    result = srcPanel.CopyTo(options, indices, NULL);
  }
  
  if (result == S_OK && useDestPanel)
  {
    UStringVector filePaths;
    UString folderPrefix;
    
    if (useTemp)
      folderPrefix = fs2us(tempDirPrefix);
    else
      folderPrefix = srcPanel.GetFsPath();
    
    filePaths.ClearAndReserve(indices.Size());
    
    FOR_VECTOR (i, indices)
    {
      UInt32 index = indices[i];
      UString s;
      if (useFullItemPaths)
        s = srcPanel.GetItemRelPath2(index);
      else
        s = srcPanel.GetItemName_for_Copy(index);
      filePaths.AddInReserved(s);
    }
    
    result = destPanel.CopyFrom(move, folderPrefix, filePaths, true, NULL);
  }
  
  if (result != S_OK)
  {
    // disableNotify1.Restore();
    // disableNotify2.Restore();
    // For Password:
    // srcPanel.SetFocusToList();
    // srcPanel.InvalidateList(NULL, true);

    if (result != E_ABORT)
      srcPanel.MessageBox_Error_HRESULT(result);
    // return;
  }

  RefreshTitleAlways();
  
  if (copyToSame || move)
  {
    srcPanel.RefreshListCtrl(srcSelState);
  }
  
  if (!copyToSame)
  {
    destPanel.RefreshListCtrl(destSelState);
    srcPanel.KillSelection();
  }

  disableNotify1.Restore();
  disableNotify2.Restore();
  srcPanel.SetFocusToList();

  // "Easy Extract": open the extracted files folder in Windows File Explorer
  if (easyExtract && openAfter && result == S_OK && !destPath.IsEmpty())
    OpenFolderInExplorer(destPath);
}

void CApp::OnSetSameFolder(unsigned srcPanelIndex)
{
  if (NumPanels <= 1)
    return;
  const CPanel &srcPanel = Panels[srcPanelIndex];
  CPanel &destPanel = Panels[1 - srcPanelIndex];
  destPanel.BindToPathAndRefresh(srcPanel._currentFolderPrefix);
}

void CApp::OnSetSubFolder(unsigned srcPanelIndex)
{
  if (NumPanels <= 1)
    return;
  const CPanel &srcPanel = Panels[srcPanelIndex];
  CPanel &destPanel = Panels[1 - srcPanelIndex];

  const int focusedItem = srcPanel._listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  const unsigned realIndex = srcPanel.GetRealItemIndex(focusedItem);
  if (!srcPanel.IsItem_Folder(realIndex))
    return;

  // destPanel.BindToFolder(srcPanel._currentFolderPrefix + srcPanel.GetItemName(realIndex) + WCHAR_PATH_SEPARATOR);

  CMyComPtr<IFolderFolder> newFolder;
  if (realIndex == kParentIndex)
  {
    if (srcPanel._folder->BindToParentFolder(&newFolder) != S_OK)
      return;
    if (!newFolder)
    {
      {
        const UString parentPrefix = srcPanel.GetParentDirPrefix();
        COpenResult openRes;
        destPanel.BindToPath(parentPrefix, UString(), openRes);
      }
      destPanel.RefreshListCtrl();
      return;
    }
  }
  else
  {
    if (srcPanel._folder->BindToFolder(realIndex, &newFolder) != S_OK)
      return;
  }

  if (!newFolder)
    return;

  destPanel.CloseOpenFolders();
  destPanel.SetNewFolder(newFolder);
  destPanel.RefreshListCtrl();
}

/*
int CApp::GetFocusedPanelIndex() const
{
  return LastFocusedPanel;
  HWND hwnd = ::GetFocus();
  for (;;)
  {
    if (hwnd == 0)
      return 0;
    for (unsigned i = 0; i < kNumPanelsMax; i++)
    {
      if (PanelsCreated[i] &&
          ((HWND)Panels[i] == hwnd || Panels[i]._listView == hwnd))
        return i;
    }
    hwnd = GetParent(hwnd);
  }
}
*/

static UString g_ToolTipBuffer;
static CSysString g_ToolTipBufferSys;

bool CApp::OnNotify(int /* ctrlID */, LPNMHDR pnmh, LRESULT &result)
{
  result = 0;
  if (_toolBar && pnmh->hwndFrom == (HWND)_toolBar && pnmh->code != NM_CUSTOMDRAW)
    if (OnToolbarAdjustNotify(pnmh, result))
      return true;
  if (_chevronBar && pnmh->hwndFrom == (HWND)_chevronBar && pnmh->code == NM_RCLICK)
  {
    ShowToolbarContextMenu();
    result = TRUE;
    return true;
  }
  if (pnmh->code == NM_CUSTOMDRAW && pnmh->hwndFrom
      && ((_toolBar && pnmh->hwndFrom == (HWND)_toolBar)
       || (_chevronBar && pnmh->hwndFrom == (HWND)_chevronBar)))
  {
    LPNMTBCUSTOMDRAW cd = (LPNMTBCUSTOMDRAW)pnmh;
    const bool isMain = (pnmh->hwndFrom == (HWND)_toolBar);
    const bool dark = NDarkMode::IsDark();
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
    {
      if (dark)
      {
        RECT rc;
        ::GetClientRect(pnmh->hwndFrom, &rc);
        ::FillRect(cd->nmcd.hdc, &rc, NDarkMode::Brush_Bk());
      }
      result = (isMain ? CDRF_NOTIFYPOSTPAINT : 0) | (dark ? CDRF_NOTIFYITEMDRAW : 0);
      return true;
    }
    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
    {
      if (!dark)
        return false;
      cd->clrText = NDarkMode::Color_Text();
      cd->clrTextHighlight = NDarkMode::Color_Text();
      cd->clrBtnFace = NDarkMode::Color_Bk();
      cd->clrHighlightHotTrack = NDarkMode::Color_Hot();
      cd->nStringBkMode = TRANSPARENT;
      cd->nHLStringBkMode = TRANSPARENT;
      result = TBCDRF_USECDCOLORS | TBCDRF_HILITEHOTTRACK;
      return true;
    }
    if (cd->nmcd.dwDrawStage == CDDS_POSTPAINT)
    {
      if (isMain)
        DrawToolbarLogo(_toolBar, _logoImageList, cd->nmcd.hdc);
      result = CDRF_DODEFAULT;
      return true;
    }
    return false;
  }
  {
    if (pnmh->code == TTN_GETDISPINFO)
    {
      LPNMTTDISPINFO info = (LPNMTTDISPINFO)pnmh;
      info->hinst = NULL;
      g_ToolTipBuffer.Empty();
      SetButtonText((int)info->hdr.idFrom, g_ToolTipBuffer);
      g_ToolTipBufferSys = GetSystemString(g_ToolTipBuffer);
      info->lpszText = g_ToolTipBufferSys.Ptr_non_const();
      return true;
    }
    #ifndef _UNICODE
    if (pnmh->code == TTN_GETDISPINFOW)
    {
      LPNMTTDISPINFOW info = (LPNMTTDISPINFOW)pnmh;
      info->hinst = NULL;
      g_ToolTipBuffer.Empty();
      SetButtonText((int)info->hdr.idFrom, g_ToolTipBuffer);
      info->lpszText = g_ToolTipBuffer.Ptr_non_const();
      return true;
    }
    #endif
  }
  return false;
}

void CApp::RefreshTitle(bool always)
{
  UString path = GetFocusedPanel()._currentFolderPrefix;
  if (path.IsEmpty())
    path = "7-Zip"; // LangString(IDS_APP_TITLE);
  if (!always && path == PrevTitle)
    return;
  PrevTitle = path;
  NWindows::MySetWindowText(_window, path);
}

void CApp::RefreshTitlePanel(unsigned panelIndex, bool always)
{
  if (panelIndex != GetFocusedPanelIndex())
    return;
  RefreshTitle(always);
}

static void AddUniqueStringToHead(UStringVector &list, const UString &s)
{
  for (unsigned i = 0; i < list.Size();)
    if (s.IsEqualTo_NoCase(list[i]))
      list.Delete(i);
    else
      i++;
  list.Insert(0, s);
}


void CFolderHistory::Normalize()
{
  const unsigned kMaxSize = 100;
  if (Strings.Size() > kMaxSize)
    Strings.DeleteFrom(kMaxSize);
}

void CFolderHistory::AddString(const UString &s)
{
  NSynchronization::CCriticalSectionLock lock(_criticalSection);
  AddUniqueStringToHead(Strings, s);
  Normalize();
}
