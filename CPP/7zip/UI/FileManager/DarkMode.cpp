// DarkMode.cpp
// Light / Dark / Auto (follow Windows) theme for 7-Zip File Manager
//
// Windows doesn't support dark mode for classic Win32 programs officially, so:
//   - title bar:   DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)
//   - popup menus: uxtheme.dll ordinal 135 SetPreferredAppMode (Windows 10 1903+)
//   - menu bar:    undocumented WM_UAHDRAWMENU / WM_UAHDRAWMENUITEM messages
//   - list view, combo box, buttons: "DarkMode_Explorer" / "DarkMode_CFD" themes
//   - other controls: our colors in WM_CTLCOLOR* and in custom drawing
// If some function is not available (old Windows), that part just stays light.

#include "StdAfx.h"

#include <commctrl.h>

#include "../../../../C/7zTypes.h"

#include "DarkMode.h"

namespace NDarkMode {

// ---------- colors ----------

static const COLORREF k_Dark_Bk      = RGB(48, 48, 48);
static const COLORREF k_Dark_Text    = RGB(230, 230, 230);
static const COLORREF k_Dark_ListBk  = RGB(40, 40, 40);
static const COLORREF k_Dark_EditBk  = RGB(60, 60, 60);
static const COLORREF k_Dark_Hot     = RGB(80, 80, 80);
static const COLORREF k_Dark_Border  = RGB(90, 90, 90);
static const COLORREF k_Dark_Select  = RGB(130, 60, 60);

static EThemeMode g_Mode = k_Theme_Light;
static bool g_IsDark = false;

static HBRUSH g_Brush_Bk;
static HBRUSH g_Brush_ListBk;
static HBRUSH g_Brush_EditBk;
static HBRUSH g_Brush_Hot;
static HBRUSH g_Brush_Border;

static HBRUSH GetBrush(HBRUSH &b, COLORREF c)
{
  if (!b)
    b = ::CreateSolidBrush(c);
  return b;
}

COLORREF Color_Bk()     { return g_IsDark ? k_Dark_Bk     : ::GetSysColor(COLOR_BTNFACE); }
COLORREF Color_Text()   { return g_IsDark ? k_Dark_Text   : ::GetSysColor(COLOR_BTNTEXT); }
COLORREF Color_ListBk() { return g_IsDark ? k_Dark_ListBk : ::GetSysColor(COLOR_WINDOW); }
COLORREF Color_Hot()    { return k_Dark_Hot; }
COLORREF Color_SelectMark() { return g_IsDark ? k_Dark_Select : RGB(255, 192, 192); }

HBRUSH Brush_Bk()     { return GetBrush(g_Brush_Bk, k_Dark_Bk); }
HBRUSH Brush_ListBk() { return GetBrush(g_Brush_ListBk, k_Dark_ListBk); }
HBRUSH Brush_EditBk() { return GetBrush(g_Brush_EditBk, k_Dark_EditBk); }
static HBRUSH Brush_Hot()    { return GetBrush(g_Brush_Hot, k_Dark_Hot); }
static HBRUSH Brush_Border() { return GetBrush(g_Brush_Border, k_Dark_Border); }


// ---------- system functions (loaded dynamically) ----------

typedef HRESULT (WINAPI *Func_SetWindowTheme)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
typedef BOOL (WINAPI *Func_AllowDarkModeForWindow)(HWND hwnd, BOOL allow);
typedef int (WINAPI *Func_SetPreferredAppMode)(int appMode);
typedef void (WINAPI *Func_FlushMenuThemes)(void);
typedef HRESULT (WINAPI *Func_DwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);

static const int k_AppMode_ForceDark  = 2;
static const int k_AppMode_ForceLight = 3;

static bool g_FuncsLoaded;
static Func_SetWindowTheme         f_SetWindowTheme;
static Func_AllowDarkModeForWindow f_AllowDarkModeForWindow;
static Func_SetPreferredAppMode    f_SetPreferredAppMode;
static Func_FlushMenuThemes        f_FlushMenuThemes;
static Func_DwmSetWindowAttribute  f_DwmSetWindowAttribute;

static void LoadFuncs()
{
  if (g_FuncsLoaded)
    return;
  g_FuncsLoaded = true;
  const HMODULE ux = ::LoadLibraryW(L"uxtheme.dll");
  if (ux)
  {
    f_SetWindowTheme = Z7_GET_PROC_ADDRESS(Func_SetWindowTheme, ux, "SetWindowTheme");
    // undocumented functions (by ordinal). They exist in Windows 10 1903+ and Windows 11.
    // RtlGetVersion returns real Windows version (GetVersionEx can return smaller version)
    typedef LONG (WINAPI *Func_RtlGetVersion)(OSVERSIONINFOW *vi);
    OSVERSIONINFOW vi;
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    bool versionOK = false;
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll)
    {
      const Func_RtlGetVersion f = Z7_GET_PROC_ADDRESS(Func_RtlGetVersion, ntdll, "RtlGetVersion");
      if (f && f(&vi) == 0)
        versionOK = (vi.dwMajorVersion >= 10 && vi.dwBuildNumber >= 18362);
    }
    if (versionOK)
    {
      f_AllowDarkModeForWindow = Z7_GET_PROC_ADDRESS(Func_AllowDarkModeForWindow, ux, MAKEINTRESOURCEA(133));
      f_SetPreferredAppMode = Z7_GET_PROC_ADDRESS(Func_SetPreferredAppMode, ux, MAKEINTRESOURCEA(135));
      f_FlushMenuThemes = Z7_GET_PROC_ADDRESS(Func_FlushMenuThemes, ux, MAKEINTRESOURCEA(136));
    }
  }
  const HMODULE dwm = ::LoadLibraryW(L"dwmapi.dll");
  if (dwm)
    f_DwmSetWindowAttribute = Z7_GET_PROC_ADDRESS(Func_DwmSetWindowAttribute, dwm, "DwmSetWindowAttribute");
}

static void SetTheme(HWND hwnd, LPCWSTR subApp, LPCWSTR idList)
{
  if (hwnd && f_SetWindowTheme)
    f_SetWindowTheme(hwnd, subApp, idList);
}

static void AllowDark(HWND hwnd, bool allow)
{
  if (hwnd && f_AllowDarkModeForWindow)
    f_AllowDarkModeForWindow(hwnd, allow ? TRUE : FALSE);
}


// ---------- mode ----------

static const wchar_t * const k_RegKey_FM = L"Software\\7-Zip\\FM";
static const wchar_t * const k_RegValue_Theme = L"Theme";

static bool ReadDword(HKEY root, LPCWSTR keyName, LPCWSTR valueName, DWORD &value)
{
  HKEY key;
  if (::RegOpenKeyExW(root, keyName, 0, KEY_READ, &key) != ERROR_SUCCESS)
    return false;
  DWORD type = 0;
  DWORD size = sizeof(value);
  const LONG res = ::RegQueryValueExW(key, valueName, NULL, &type, (LPBYTE)&value, &size);
  ::RegCloseKey(key);
  return (res == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(value));
}

static void WriteDword(HKEY root, LPCWSTR keyName, LPCWSTR valueName, DWORD value)
{
  HKEY key;
  if (::RegCreateKeyExW(root, keyName, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
    return;
  ::RegSetValueExW(key, valueName, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
  ::RegCloseKey(key);
}

static bool IsSystemDark()
{
  DWORD v = 1;
  if (!ReadDword(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"AppsUseLightTheme", v))
    return false;
  return (v == 0);
}

static bool CalcDark()
{
  if (g_Mode == k_Theme_Dark)
    return true;
  if (g_Mode == k_Theme_Auto)
    return IsSystemDark();
  return false;
}

static void ApplyAppMode()
{
  if (f_SetPreferredAppMode)
    f_SetPreferredAppMode(g_IsDark ? k_AppMode_ForceDark : k_AppMode_ForceLight);
  if (f_FlushMenuThemes)
    f_FlushMenuThemes();
}

void Init()
{
  LoadFuncs();
  DWORD v = 0;
  if (ReadDword(HKEY_CURRENT_USER, k_RegKey_FM, k_RegValue_Theme, v))
  {
    if (v == (DWORD)k_Theme_Dark)
      g_Mode = k_Theme_Dark;
    else if (v == (DWORD)k_Theme_Auto)
      g_Mode = k_Theme_Auto;
    else
      g_Mode = k_Theme_Light;
  }
  g_IsDark = CalcDark();
  ApplyAppMode();
}

EThemeMode GetMode() { return g_Mode; }
bool IsDark() { return g_IsDark; }

bool SetMode(EThemeMode mode)
{
  g_Mode = mode;
  WriteDword(HKEY_CURRENT_USER, k_RegKey_FM, k_RegValue_Theme, (DWORD)mode);
  const bool dark = CalcDark();
  const bool changed = (dark != g_IsDark);
  g_IsDark = dark;
  ApplyAppMode();
  return changed;
}

bool UpdateFromSystem()
{
  if (g_Mode != k_Theme_Auto)
    return false;
  const bool dark = CalcDark();
  if (dark == g_IsDark)
    return false;
  g_IsDark = dark;
  ApplyAppMode();
  return true;
}

bool IsThemeSettingChange(LPARAM lParam)
{
  if (lParam == 0)
    return false;
  return (::lstrcmpW((LPCWSTR)lParam, L"ImmersiveColorSet") == 0);
}


// ---------- title bar ----------

void ApplyToTitleBar(HWND hwnd)
{
  if (!hwnd || !f_DwmSetWindowAttribute)
    return;
  const BOOL value = g_IsDark ? TRUE : FALSE;
  // 20 : DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1+, Windows 11)
  // 19 : the same attribute in older Windows 10 builds
  if (FAILED(f_DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value))))
    f_DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));
}

void ApplyToMainWindow(HWND hwnd)
{
  LoadFuncs();
  AllowDark(hwnd, g_IsDark);
  ApplyToTitleBar(hwnd);
  ApplyAppMode();
  ::DrawMenuBar(hwnd);
}


// ---------- subclassing of controls ----------
// One window procedure for several controls:
//   k_Sub_Bk       : WM_ERASEBKGND : dark background
//   k_Sub_CtlColor : WM_CTLCOLOR* from child edit / list (combo box)
//   k_Sub_Status   : WM_PAINT : status bar with dark background and light text

static const UINT k_Sub_Bk       = 1 << 0;
static const UINT k_Sub_CtlColor = 1 << 1;
static const UINT k_Sub_Status   = 1 << 2;
static const UINT k_Sub_ListHeader = 1 << 3; // list view: light text in column headers
static const UINT k_Sub_Tab      = 1 << 4;  // tab control: we draw dark tabs
static const UINT k_Sub_Sheet    = 1 << 5;  // property sheet frame (Options window)
static const UINT k_Sub_ComboEx  = 1 << 6;  // ComboBoxEx: we draw items of drop-down list

static const wchar_t * const k_Prop_OldProc = L"Z7_Dark_OldProc";
static const wchar_t * const k_Prop_Flags = L"Z7_Dark_Flags";

static void PaintStatusBar(HWND hwnd)
{
  PAINTSTRUCT ps;
  const HDC dc = ::BeginPaint(hwnd, &ps);
  if (!dc)
    return;
  RECT rc;
  ::GetClientRect(hwnd, &rc);
  ::FillRect(dc, &rc, Brush_Bk());

  const HFONT font = (HFONT)::SendMessageW(hwnd, WM_GETFONT, 0, 0);
  const HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
  ::SetBkMode(dc, TRANSPARENT);
  ::SetTextColor(dc, k_Dark_Text);

  const int numParts = (int)::SendMessageW(hwnd, SB_GETPARTS, 0, 0);
  for (int i = 0; i < numParts; i++)
  {
    RECT pr;
    if (!::SendMessageW(hwnd, SB_GETRECT, (WPARAM)i, (LPARAM)&pr))
      continue;
    const unsigned len = LOWORD(::SendMessageW(hwnd, SB_GETTEXTLENGTHW, (WPARAM)i, 0));
    wchar_t buf[1024];
    buf[0] = 0;
    if (len < Z7_ARRAY_SIZE(buf))
      ::SendMessageW(hwnd, SB_GETTEXTW, (WPARAM)i, (LPARAM)buf);
    RECT tr = pr;
    tr.left += 4;
    tr.right -= 2;
    ::DrawTextW(dc, buf, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (i + 1 < numParts)
    {
      RECT line = pr;
      line.left = pr.right - 1;
      line.top += 3;
      line.bottom -= 3;
      ::FillRect(dc, &line, Brush_Border());
    }
  }
  if (oldFont)
    ::SelectObject(dc, oldFont);
  ::EndPaint(hwnd, &ps);
}

static void PaintTabControl(HWND hwnd)
{
  PAINTSTRUCT ps;
  const HDC dc = ::BeginPaint(hwnd, &ps);
  if (!dc)
    return;
  RECT rc;
  ::GetClientRect(hwnd, &rc);
  ::FillRect(dc, &rc, Brush_Bk());

  // frame around the page area
  {
    RECT page = rc;
    TabCtrl_AdjustRect(hwnd, FALSE, &page);
    ::InflateRect(&page, 2, 2);
    ::FrameRect(dc, &page, Brush_Border());
  }

  const HFONT font = (HFONT)::SendMessageW(hwnd, WM_GETFONT, 0, 0);
  const HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
  ::SetBkMode(dc, TRANSPARENT);
  ::SetTextColor(dc, k_Dark_Text);

  const int numItems = (int)::SendMessageW(hwnd, TCM_GETITEMCOUNT, 0, 0);
  const int sel = (int)::SendMessageW(hwnd, TCM_GETCURSEL, 0, 0);
  for (int i = 0; i < numItems; i++)
  {
    RECT ir;
    if (!::SendMessageW(hwnd, TCM_GETITEMRECT, (WPARAM)i, (LPARAM)&ir))
      continue;
    if (i == sel)
      ir.top -= 2;
    ::FillRect(dc, &ir, i == sel ? Brush_EditBk() : Brush_Bk());
    ::FrameRect(dc, &ir, Brush_Border());

    wchar_t text[128];
    text[0] = 0;
    TCITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = TCIF_TEXT;
    item.pszText = text;
    item.cchTextMax = (int)Z7_ARRAY_SIZE(text);
    ::SendMessageW(hwnd, TCM_GETITEMW, (WPARAM)i, (LPARAM)&item);
    ::DrawTextW(dc, text, -1, &ir, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
  }
  if (oldFont)
    ::SelectObject(dc, oldFont);
  ::EndPaint(hwnd, &ps);
}

// ComboBoxEx (path box): items of drop-down list (icon, indent, text) with dark colors
static void DrawComboBoxExItem(HWND comboBoxEx, const DRAWITEMSTRUCT *dis)
{
  const HDC dc = dis->hDC;
  const bool isEditPart = ((dis->itemState & ODS_COMBOBOXEDIT) != 0);
  const bool selected = !isEditPart && (dis->itemState & ODS_SELECTED) != 0;
  RECT rc = dis->rcItem;
  ::FillRect(dc, &rc, selected ? Brush_Hot() : Brush_EditBk());

  wchar_t text[1024];
  text[0] = 0;
  COMBOBOXEXITEMW item;
  memset(&item, 0, sizeof(item));
  item.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_INDENT;
  item.iItem = isEditPart ? -1 : (INT_PTR)(int)dis->itemID;
  item.pszText = text;
  item.cchTextMax = (int)Z7_ARRAY_SIZE(text);
  if (!isEditPart && (int)dis->itemID < 0)
    return;
  if (!::SendMessageW(comboBoxEx, CBEM_GETITEMW, 0, (LPARAM)&item))
    return;

  int x = rc.left + 2;
  if (!isEditPart && item.iIndent > 0)
    x += item.iIndent * 10;
  const HIMAGELIST il = (HIMAGELIST)::SendMessageW(comboBoxEx, CBEM_GETIMAGELIST, 0, 0);
  if (il)
  {
    int cx = 0, cy = 0;
    ImageList_GetIconSize(il, &cx, &cy);
    const int image = selected ? item.iSelectedImage : item.iImage;
    if (image >= 0)
      ImageList_Draw(il, image, dc, x, rc.top + (rc.bottom - rc.top - cy) / 2, ILD_TRANSPARENT);
    x += cx + 4;
  }
  if (isEditPart)
    return; // the text is in edit control
  ::SetBkMode(dc, TRANSPARENT);
  ::SetTextColor(dc, k_Dark_Text);
  RECT tr = rc;
  tr.left = x;
  ::DrawTextW(dc, text, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  const WNDPROC oldProc = (WNDPROC)::GetPropW(hwnd, k_Prop_OldProc);
  const UINT flags = (UINT)(UINT_PTR)::GetPropW(hwnd, k_Prop_Flags);
  if (g_IsDark)
  {
    switch (message)
    {
      case WM_ERASEBKGND:
        if (flags & (k_Sub_Bk | k_Sub_Status | k_Sub_Tab | k_Sub_Sheet))
        {
          RECT rc;
          ::GetClientRect(hwnd, &rc);
          ::FillRect((HDC)wParam, &rc, Brush_Bk());
          return 1;
        }
        break;
      case WM_PAINT:
        if (flags & k_Sub_Status)
        {
          PaintStatusBar(hwnd);
          return 0;
        }
        if (flags & k_Sub_Tab)
        {
          PaintTabControl(hwnd);
          return 0;
        }
        break;
      case WM_DRAWITEM:
        if (flags & k_Sub_ComboEx)
        {
          const DRAWITEMSTRUCT *dis = (const DRAWITEMSTRUCT *)lParam;
          if (dis && dis->CtlType == ODT_COMBOBOX)
          {
            DrawComboBoxExItem(hwnd, dis);
            return TRUE;
          }
        }
        break;
      case WM_NOTIFY:
        if (flags & k_Sub_ListHeader)
        {
          const NMHDR *h = (const NMHDR *)lParam;
          if (h && h->code == NM_CUSTOMDRAW && h->hwndFrom == ListView_GetHeader(hwnd))
          {
            const NMCUSTOMDRAW *cd = (const NMCUSTOMDRAW *)lParam;
            if (cd->dwDrawStage == CDDS_PREPAINT)
              return CDRF_NOTIFYITEMDRAW;
            if (cd->dwDrawStage == CDDS_ITEMPREPAINT)
            {
              ::SetTextColor(cd->hdc, k_Dark_Text);
              return CDRF_DODEFAULT;
            }
          }
        }
        break;
      case WM_CTLCOLORDLG:
      case WM_CTLCOLORSTATIC:
      case WM_CTLCOLORBTN:
        if (flags & k_Sub_Sheet)
        {
          ::SetTextColor((HDC)wParam, k_Dark_Text);
          ::SetBkColor((HDC)wParam, k_Dark_Bk);
          return (LRESULT)Brush_Bk();
        }
        break;
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORLISTBOX:
        if (flags & k_Sub_CtlColor)
        {
          ::SetTextColor((HDC)wParam, k_Dark_Text);
          ::SetBkColor((HDC)wParam, k_Dark_EditBk);
          return (LRESULT)Brush_EditBk();
        }
        break;
      default:
        break;
    }
  }
  if (message == WM_NCDESTROY)
  {
    ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)oldProc);
    ::RemovePropW(hwnd, k_Prop_OldProc);
    ::RemovePropW(hwnd, k_Prop_Flags);
  }
  if (!oldProc)
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
  return ::CallWindowProcW(oldProc, hwnd, message, wParam, lParam);
}

static void Subclass(HWND hwnd, UINT flags)
{
  if (!hwnd)
    return;
  if (::GetPropW(hwnd, k_Prop_OldProc))
  {
    // already subclassed: just add flags
    const UINT old = (UINT)(UINT_PTR)::GetPropW(hwnd, k_Prop_Flags);
    ::SetPropW(hwnd, k_Prop_Flags, (HANDLE)(UINT_PTR)(old | flags));
    return;
  }
  ::SetPropW(hwnd, k_Prop_Flags, (HANDLE)(UINT_PTR)flags);
  const LONG_PTR oldProc = ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)SubclassProc);
  ::SetPropW(hwnd, k_Prop_OldProc, (HANDLE)oldProc);
}


// ---------- controls ----------

void ApplyToListView(HWND listView)
{
  if (!listView)
    return;
  LoadFuncs();
  AllowDark(listView, g_IsDark);
  SetTheme(listView, g_IsDark ? L"DarkMode_Explorer" : NULL, NULL);
  Subclass(listView, k_Sub_ListHeader);
  const COLORREF bk = g_IsDark ? k_Dark_ListBk : ::GetSysColor(COLOR_WINDOW);
  const COLORREF text = g_IsDark ? k_Dark_Text : ::GetSysColor(COLOR_WINDOWTEXT);
  ListView_SetBkColor(listView, bk);
  ListView_SetTextBkColor(listView, bk);
  ListView_SetTextColor(listView, text);
  const HWND header = ListView_GetHeader(listView);
  if (header)
  {
    AllowDark(header, g_IsDark);
    SetTheme(header, g_IsDark ? L"DarkMode_ItemsView" : NULL, NULL);
    ::InvalidateRect(header, NULL, TRUE);
  }
  ::InvalidateRect(listView, NULL, TRUE);
}

void ApplyToComboBoxEx(HWND comboBoxEx)
{
  if (!comboBoxEx)
    return;
  LoadFuncs();
  const HWND combo = (HWND)::SendMessageW(comboBoxEx, CBEM_GETCOMBOCONTROL, 0, 0);
  const HWND edit = (HWND)::SendMessageW(comboBoxEx, CBEM_GETEDITCONTROL, 0, 0);
  const LPCWSTR theme = g_IsDark ? L"DarkMode_CFD" : NULL;
  AllowDark(comboBoxEx, g_IsDark);
  AllowDark(combo, g_IsDark);
  AllowDark(edit, g_IsDark);
  SetTheme(combo, theme, NULL);
  SetTheme(edit, theme, NULL);
  Subclass(combo, k_Sub_CtlColor);
  Subclass(comboBoxEx, k_Sub_CtlColor | k_Sub_ComboEx);
  if (combo)
  {
    // drop-down list window: dark scroll bar
    COMBOBOXINFO cbi;
    memset(&cbi, 0, sizeof(cbi));
    cbi.cbSize = sizeof(cbi);
    if (::GetComboBoxInfo(combo, &cbi) && cbi.hwndList)
    {
      AllowDark(cbi.hwndList, g_IsDark);
      SetTheme(cbi.hwndList, g_IsDark ? L"DarkMode_Explorer" : NULL, NULL);
    }
  }
  ::InvalidateRect(comboBoxEx, NULL, TRUE);
  if (combo)
    ::InvalidateRect(combo, NULL, TRUE);
  if (edit)
    ::InvalidateRect(edit, NULL, TRUE);
}

void ApplyToStatusBar(HWND statusBar)
{
  Subclass(statusBar, k_Sub_Status);
  if (statusBar)
    ::InvalidateRect(statusBar, NULL, TRUE);
}

void ApplyToBkWindow(HWND hwnd)
{
  Subclass(hwnd, k_Sub_Bk);
  if (hwnd)
    ::InvalidateRect(hwnd, NULL, TRUE);
}


// ---------- menu bar (main window) ----------
// undocumented messages, that are sent to the window with menu bar,
// if the menu bar is drawn with visual styles (Windows Vista+)

#define Z7_WM_UAHDRAWMENU       0x0091
#define Z7_WM_UAHDRAWMENUITEM   0x0092

struct CUahMenu
{
  HMENU hmenu;
  HDC hdc;
  DWORD dwFlags;
};

union CUahMenuItemMetrics
{
  struct { DWORD cx; DWORD cy; } rgsizeBar[2];
  struct { DWORD cx; DWORD cy; } rgsizePopup[4];
};

struct CUahMenuPopupMetrics
{
  DWORD rgcx[4];
  DWORD fUpdateMaxWidths;
};

struct CUahMenuItem
{
  int iPosition;
  CUahMenuItemMetrics umim;
  CUahMenuPopupMetrics umpm;
};

struct CUahDrawMenuItem
{
  DRAWITEMSTRUCT dis;
  CUahMenu um;
  CUahMenuItem umi;
};

static bool GetMenuBarRect(HWND hwnd, RECT &rc)
{
  MENUBARINFO mbi;
  memset(&mbi, 0, sizeof(mbi));
  mbi.cbSize = sizeof(mbi);
  if (!::GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    return false;
  RECT wr;
  ::GetWindowRect(hwnd, &wr);
  rc = mbi.rcBar;
  ::OffsetRect(&rc, -wr.left, -wr.top);
  return true;
}

// Windows draws light line between menu bar and client area. We draw over it.
static void DrawMenuBarBottomLine(HWND hwnd)
{
  MENUBARINFO mbi;
  memset(&mbi, 0, sizeof(mbi));
  mbi.cbSize = sizeof(mbi);
  if (!::GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    return;
  RECT rcClient;
  ::GetClientRect(hwnd, &rcClient);
  ::MapWindowPoints(hwnd, NULL, (POINT *)(void *)&rcClient, 2);
  RECT rcWindow;
  ::GetWindowRect(hwnd, &rcWindow);
  ::OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);
  RECT line = rcClient;
  line.bottom = line.top;
  line.top--;
  const HDC dc = ::GetWindowDC(hwnd);
  if (dc)
  {
    ::FillRect(dc, &line, Brush_Bk());
    ::ReleaseDC(hwnd, dc);
  }
}

static void DrawMenuBarItem(CUahDrawMenuItem *p)
{
  wchar_t text[256];
  text[0] = 0;
  MENUITEMINFOW mii;
  memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STRING;
  mii.dwTypeData = text;
  mii.cch = (UINT)Z7_ARRAY_SIZE(text) - 1;
  ::GetMenuItemInfoW(p->um.hmenu, (UINT)p->umi.iPosition, TRUE, &mii);

  const UINT state = p->dis.itemState;
  HBRUSH bk = Brush_Bk();
  if (state & (ODS_HOTLIGHT | ODS_SELECTED))
    bk = Brush_Hot();
  ::FillRect(p->um.hdc, &p->dis.rcItem, bk);

  COLORREF textColor = k_Dark_Text;
  if (state & (ODS_INACTIVE | ODS_GRAYED | ODS_DISABLED))
    textColor = RGB(140, 140, 140);
  UINT format = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
  if (state & ODS_NOACCEL)
    format |= DT_HIDEPREFIX;
  ::SetBkMode(p->um.hdc, TRANSPARENT);
  ::SetTextColor(p->um.hdc, textColor);
  ::DrawTextW(p->um.hdc, text, -1, &p->dis.rcItem, format);
}

bool OnMainWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT &result)
{
  result = 0;
  if (!g_IsDark)
    return false;
  switch (message)
  {
    case Z7_WM_UAHDRAWMENU:
    {
      const CUahMenu *p = (const CUahMenu *)lParam;
      RECT rc;
      if (p && GetMenuBarRect(hwnd, rc))
        ::FillRect(p->hdc, &rc, Brush_Bk());
      return true;
    }
    case Z7_WM_UAHDRAWMENUITEM:
    {
      CUahDrawMenuItem *p = (CUahDrawMenuItem *)lParam;
      if (p)
        DrawMenuBarItem(p);
      return true;
    }
    case WM_NCPAINT:
    case WM_NCACTIVATE:
      result = ::DefWindowProcW(hwnd, message, wParam, lParam);
      DrawMenuBarBottomLine(hwnd);
      return true;
    case WM_ERASEBKGND:
    {
      RECT rc;
      ::GetClientRect(hwnd, &rc);
      ::FillRect((HDC)wParam, &rc, Brush_Bk());
      result = 1;
      return true;
    }
    default:
      break;
  }
  return false;
}


// ---------- dialogs ----------

// Options window: frame of property sheet (tabs, OK / Cancel / Apply / Help buttons)

static const wchar_t * const k_Prop_SheetDone = L"Z7_Dark_Sheet";

static BOOL CALLBACK SheetChildProc(HWND hwnd, LPARAM lParam)
{
  if (::GetParent(hwnd) != (HWND)lParam)
    return TRUE; // only direct children of sheet. Pages are processed separately.
  wchar_t name[64];
  name[0] = 0;
  ::GetClassNameW(hwnd, name, (int)Z7_ARRAY_SIZE(name));
  if (::lstrcmpiW(name, L"Button") == 0)
  {
    AllowDark(hwnd, true);
    SetTheme(hwnd, L"DarkMode_Explorer", NULL);
  }
  else if (::lstrcmpiW(name, L"SysTabControl32") == 0)
    Subclass(hwnd, k_Sub_Tab);
  ::InvalidateRect(hwnd, NULL, TRUE);
  return TRUE;
}

static void ApplyToPropertySheet(HWND sheet)
{
  if (!sheet || ::GetPropW(sheet, k_Prop_SheetDone))
    return;
  if (!::FindWindowExW(sheet, NULL, L"SysTabControl32", NULL))
    return; // it's not property sheet
  ::SetPropW(sheet, k_Prop_SheetDone, (HANDLE)(UINT_PTR)1);
  AllowDark(sheet, true);
  ApplyToTitleBar(sheet);
  Subclass(sheet, k_Sub_Sheet);
  ::EnumChildWindows(sheet, SheetChildProc, (LPARAM)sheet);
  ::InvalidateRect(sheet, NULL, TRUE);
}

static BOOL CALLBACK DialogChildProc(HWND hwnd, LPARAM /* lParam */)
{
  wchar_t name[64];
  name[0] = 0;
  ::GetClassNameW(hwnd, name, (int)Z7_ARRAY_SIZE(name));

  if (::lstrcmpiW(name, L"Button") == 0)
  {
    const LONG_PTR type = ::GetWindowLongPtrW(hwnd, GWL_STYLE) & BS_TYPEMASK;
    if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON)
    {
      AllowDark(hwnd, true);
      SetTheme(hwnd, L"DarkMode_Explorer", NULL);
    }
    else if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX
        || type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON
        || type == BS_3STATE || type == BS_AUTO3STATE
        || type == BS_GROUPBOX)
    {
      // check box, radio button, group box:
      // themed control ignores our text color, so we use classic look.
      SetTheme(hwnd, L"", L"");
    }
  }
  else if (::lstrcmpiW(name, L"Edit") == 0)
  {
    AllowDark(hwnd, true);
    SetTheme(hwnd, L"DarkMode_CFD", NULL);
  }
  else if (::lstrcmpiW(name, L"ComboBox") == 0)
  {
    AllowDark(hwnd, true);
    SetTheme(hwnd, L"DarkMode_CFD", NULL);
    Subclass(hwnd, k_Sub_CtlColor);
  }
  else if (::lstrcmpiW(name, L"ComboBoxEx32") == 0)
    ApplyToComboBoxEx(hwnd);
  else if (::lstrcmpiW(name, L"SysListView32") == 0)
    ApplyToListView(hwnd);
  else if (::lstrcmpiW(name, L"SysTreeView32") == 0)
  {
    AllowDark(hwnd, true);
    SetTheme(hwnd, L"DarkMode_Explorer", NULL);
    TreeView_SetBkColor(hwnd, k_Dark_ListBk);
    TreeView_SetTextColor(hwnd, k_Dark_Text);
  }
  else if (::lstrcmpiW(name, L"ScrollBar") == 0)
  {
    AllowDark(hwnd, true);
    SetTheme(hwnd, L"DarkMode_Explorer", NULL);
  }
  return TRUE;
}

bool DialogHook(HWND hwnd, UINT message, WPARAM wParam, LPARAM /* lParam */, LRESULT &result)
{
  result = 0;
  if (!g_IsDark)
    return false;
  switch (message)
  {
    case WM_INITDIALOG:
    {
      LoadFuncs();
      AllowDark(hwnd, true);
      if ((::GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) == 0)
        ApplyToTitleBar(hwnd);
      else
        ApplyToPropertySheet(::GetParent(hwnd)); // page of Options window
      ::EnumChildWindows(hwnd, DialogChildProc, 0);
      return false; // the dialog itself must process WM_INITDIALOG
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
      const HDC dc = (HDC)wParam;
      ::SetTextColor(dc, k_Dark_Text);
      ::SetBkColor(dc, k_Dark_Bk);
      result = (LRESULT)Brush_Bk();
      return true;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    {
      const HDC dc = (HDC)wParam;
      ::SetTextColor(dc, k_Dark_Text);
      ::SetBkColor(dc, k_Dark_EditBk);
      result = (LRESULT)Brush_EditBk();
      return true;
    }
    default:
      break;
  }
  return false;
}

}
