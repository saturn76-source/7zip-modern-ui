// Panel.cpp

#include "StdAfx.h"

#include <windowsx.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/Thread.h"

#include "../../PropID.h"

#include "resource.h"
#include "../GUI/ExtractRes.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ZipRegistry.h"

#include "../Agent/IFolderArchive.h"

#include "App.h"
#include "ExtractCallback.h"
#include "FSFolder.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "Panel.h"
#include "DarkMode.h"
#include "CopyDialog.h"
#include "RootFolder.h"

#include "PropertyNameRes.h"

using namespace NWindows;
using namespace NControl;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

static const UINT_PTR kTimerID = 1;
static const UINT kTimerElapse = 1000;

static DWORD kStyles[4] = { LVS_ICON, LVS_SMALLICON, LVS_LIST, LVS_REPORT };

// two small buttons at the right end of status bar (like in Windows Explorer):
// "Details" and "Large Icons" view
static const int kViewBtnID_Details = 0x6F01;
static const int kViewBtnID_Icons   = 0x6F02;
static const UInt32 kViewMode_Icons   = 0; // index in kStyles[]
static const UInt32 kViewMode_Details = 3;

static void DrawViewButton(const DRAWITEMSTRUCT *dis, bool isDetails, bool checked)
{
  const HDC dc = dis->hDC;
  const RECT rc = dis->rcItem;
  const bool dark = NDarkMode::IsDark();
  {
    const HBRUSH br = ::CreateSolidBrush(dark ? NDarkMode::Color_Bk() : ::GetSysColor(COLOR_BTNFACE));
    ::FillRect(dc, &rc, br);
    ::DeleteObject(br);
  }
  const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
  if (checked || pressed)
  {
    const COLORREF fill = dark ? RGB(85, 85, 85) : RGB(204, 228, 247);
    const COLORREF line = dark ? RGB(120, 120, 120) : RGB(153, 209, 255);
    const HBRUSH br = ::CreateSolidBrush(fill);
    const HPEN pen = ::CreatePen(PS_SOLID, 1, line);
    const HGDIOBJ oldBr = ::SelectObject(dc, br);
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::RoundRect(dc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 6, 6);
    ::SelectObject(dc, oldBr);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(br);
    ::DeleteObject(pen);
  }
  // icon
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  int s = (w < h ? w : h) * 6 / 10; // icon size
  if (s < 8)
    s = 8;
  const int x0 = rc.left + (w - s) / 2;
  const int y0 = rc.top + (h - s) / 2;
  const HBRUSH br = ::CreateSolidBrush(dark ? RGB(225, 225, 225) : RGB(60, 60, 60));
  if (isDetails)
  {
    // 3 rows: small square + line
    const int rowH = s / 5 > 1 ? s / 5 : 2;
    for (int i = 0; i < 3; i++)
    {
      const int y = y0 + i * (s - rowH) / 2;
      RECT r1 = { x0, y, x0 + rowH, y + rowH };
      RECT r2 = { x0 + rowH * 2, y, x0 + s, y + rowH };
      ::FillRect(dc, &r1, br);
      ::FillRect(dc, &r2, br);
    }
  }
  else
  {
    // 2 x 2 tiles
    const int gap = s / 6 > 1 ? s / 6 : 2;
    const int t = (s - gap) / 2;
    for (int i = 0; i < 4; i++)
    {
      const int x = x0 + (i % 2) * (t + gap);
      const int y = y0 + (i / 2) * (t + gap);
      RECT r = { x, y, x + t, y + t };
      ::FillRect(dc, &r, br);
    }
  }
  ::DeleteObject(br);
}

// static const int kCreateFolderID = 101;

extern HINSTANCE g_hInstance;

void CPanel::ReleasePanel()
{
  Disable_Processing_Timer_Notify_StatusBar();
  // It's for unloading COM dll's: don't change it.
  CloseOpenFolders();
  _sevenZipContextMenu.Release();
  _systemContextMenu.Release();
}

CPanel::~CPanel()
{
  CloseOpenFolders();
}

HWND CPanel::GetParent() const
{
  const HWND h = CWindow2::GetParent();
  return h ? h : _mainWindow;
}

#define kClassName L"7-Zip::Panel"


HRESULT CPanel::Create(HWND mainWindow, HWND parentWindow, UINT id,
    const UString &currentFolderPrefix,
    const UString &arcFormat,
    CPanelCallback *panelCallback, CAppState *appState,
    bool needOpenArc,
    COpenResult &openRes)
{
  _mainWindow = mainWindow;
  _processTimer = true;
  _processNotify = true;
  _processStatusBar = true;

  _panelCallback = panelCallback;
  _appState = appState;
  // _index = index;
  _baseID = id;
  _comboBoxID = _baseID + 3;
  _statusBarID = _comboBoxID + 1;

  UString cfp = currentFolderPrefix;

  if (!currentFolderPrefix.IsEmpty())
    if (currentFolderPrefix[0] == L'.')
    {
      FString cfpF;
      if (NFile::NDir::MyGetFullPathName(us2fs(currentFolderPrefix), cfpF))
        cfp = fs2us(cfpF);
    }

  RINOK(BindToPath(cfp, arcFormat, openRes))

  if (needOpenArc && !openRes.ArchiveIsOpened)
    return S_OK;

  if (!CreateEx(0, kClassName, NULL, WS_CHILD | WS_VISIBLE,
      0, 0, _xSize, 260,
      parentWindow, (HMENU)(UINT_PTR)id, g_hInstance))
    return E_FAIL;
  PanelCreated = true;

  return S_OK;
}

// extern UInt32 g_NumMessages;

LRESULT CPanel::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // g_NumMessages++;
  switch (message)
  {
    case kShiftSelectMessage:
      OnShiftSelectMessage();
      return 0;
    case kReLoadMessage:
      RefreshListCtrl(_selectedState);
      return 0;
    case kSetFocusToListView:
      _listView.SetFocus();
      return 0;
    case kOpenItemChanged:
      return OnOpenItemChanged(lParam);
    case kRefresh_StatusBar:
      if (_processStatusBar)
        Refresh_StatusBar();
      return 0;
    case kOpenDroppedArchiveMessage:
    {
      const UString path = g_DroppedArchivePath;
      g_DroppedArchivePath.Empty();
      if (!path.IsEmpty())
      {
        BindToPathAndRefresh(path);
        _listView.SetFocus();
      }
      return 0;
    }
    #ifdef UNDER_CE
    case kRefresh_HeaderComboBox:
      LoadFullPathAndShow();
      return 0;
    #endif
    case WM_TIMER:
      OnTimer();
      return 0;
    case WM_CONTEXTMENU:
      if (OnContextMenu(HANDLE(wParam), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        return 0;
      break;
    case WM_DRAWITEM:
    {
      const DRAWITEMSTRUCT *dis = (const DRAWITEMSTRUCT *)lParam;
      if (dis && (dis->CtlID == (UINT)kViewBtnID_Details || dis->CtlID == (UINT)kViewBtnID_Icons))
      {
        const bool isDetails = (dis->CtlID == (UINT)kViewBtnID_Details);
        const UInt32 mode = GetListViewMode();
        DrawViewButton(dis, isDetails, isDetails ? (mode == kViewMode_Details) : (mode == kViewMode_Icons));
        return TRUE;
      }
      break;
    }
    /*
    case WM_DROPFILES:
      CompressDropFiles(HDROP(wParam));
      return 0;
    */
  }
  return CWindow2::OnMessage(message, wParam, lParam);
}

LRESULT CMyListView::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_CHAR)
  {
    UINT scanCode = (UINT)((lParam >> 16) & 0xFF);
    bool extended = ((lParam & 0x1000000) != 0);
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_MULTIPLY || virtualKey == VK_ADD ||
        virtualKey == VK_SUBTRACT)
      return 0;
    if ((wParam == '/' && extended)
        || wParam == '\\' || wParam == '/')
    {
      _panel->OpenDrivesFolder();
      return 0;
    }
  }
  else if (message == WM_SYSCHAR)
  {
    // For Alt+Enter Beep disabling
    UINT scanCode = (UINT)(lParam >> 16) & 0xFF;
    UINT virtualKey = MapVirtualKey(scanCode, 1);
    if (virtualKey == VK_RETURN || virtualKey == VK_MULTIPLY ||
        virtualKey == VK_ADD || virtualKey == VK_SUBTRACT)
      return 0;
  }
  /*
  else if (message == WM_SYSKEYDOWN)
  {
    // return 0;
  }
  */
  else if (message == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0)
  {
    // Ctrl + mouse wheel: zoom file list
    const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta != 0)
      ::SendMessageW(g_HWND, WM_COMMAND, delta > 0 ? IDM_VIEW_ZOOM_IN : IDM_VIEW_ZOOM_OUT, 0);
    return 0;
  }
  else if (message == WM_KEYDOWN)
  {
    bool alt = IsKeyDown(VK_MENU);
    bool ctrl = IsKeyDown(VK_CONTROL);
    bool shift = IsKeyDown(VK_SHIFT);
    switch (wParam)
    {
      /*
      case VK_RETURN:
      {
        if (shift && !alt && !ctrl)
        {
          _panel->OpenSelectedItems(false);
          return 0;
        }
        break;
      }
      */
      case VK_NEXT:
      {
        if (ctrl && !alt && !shift)
        {
          _panel->OpenFocusedItemAsInternal();
          return 0;
        }
        break;
      }
      case VK_PRIOR:
      if (ctrl && !alt && !shift)
      {
        _panel->OpenParentFolder();
        return 0;
      }
    }
  }
  #ifdef UNDER_CE
  else if (message == WM_KEYUP)
  {
    if (wParam == VK_F2) // it's VK_TSOFT2
    {
      // Activate Menu
      ::PostMessage(g_HWND, WM_SYSCOMMAND, SC_KEYMENU, 0);
      return 0;
    }
  }
  #endif
  else if (message == WM_SETFOCUS)
  {
    _panel->_lastFocusedIsList = true;
    _panel->_panelCallback->PanelWasFocused();
  }
  return CListView2::OnMessage(message, wParam, lParam);
}

/*
static LRESULT APIENTRY ComboBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBox *w = (CMyComboBox *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

LRESULT CMyComboBox::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}
*/

#ifndef UNDER_CE

static LRESULT APIENTRY ComboBoxEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CWindow tempDialog(hwnd);
  CMyComboBoxEdit *w = (CMyComboBoxEdit *)(tempDialog.GetUserDataLongPtr());
  if (w == NULL)
    return 0;
  return w->OnMessage(message, wParam, lParam);
}

#endif

LRESULT CMyComboBoxEdit::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  // See MSDN / Subclassing a Combo Box / Creating a Combo-box Toolbar
  switch (message)
  {
    case WM_SYSKEYDOWN:
      switch (wParam)
      {
        case VK_F1:
        case VK_F2:
        {
          // check ALT
          if ((lParam & (1<<29)) == 0)
            break;
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (alt && !ctrl && !shift)
          {
            _panel->_panelCallback->SetFocusToPath(wParam == VK_F1 ? 0 : 1);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_KEYDOWN:
      switch (wParam)
      {
        case VK_TAB:
          // SendMessage(hwndMain, WM_ENTER, 0, 0);
          _panel->SetFocusToList();
          return 0;
        case VK_F9:
        {
          bool alt = IsKeyDown(VK_MENU);
          bool ctrl = IsKeyDown(VK_CONTROL);
          bool shift = IsKeyDown(VK_SHIFT);
          if (!alt && !ctrl && !shift)
          {
            g_App.SwitchOnOffOnePanel();
            return 0;
          }
          break;
        }
        case 'W':
        {
          bool ctrl = IsKeyDown(VK_CONTROL);
          if (ctrl)
          {
            PostMessage(g_HWND, WM_COMMAND, IDCLOSE, 0);
            return 0;
          }
          break;
        }
      }
      break;
    case WM_CHAR:
      switch (wParam)
      {
        case VK_TAB:
        case VK_ESCAPE:
          return 0;
      }
  }
  #ifndef _UNICODE
  if (g_IsNT)
    return CallWindowProcW(_origWindowProc, *this, message, wParam, lParam);
  else
  #endif
    return CallWindowProc(_origWindowProc, *this, message, wParam, lParam);
}


/*
  REBARBANDINFO in vista (_WIN32_WINNT >= 0x0600) has additional fields
  we want 2000/xp compatibility.
  so we must use reduced structure, if we compile with (_WIN32_WINNT >= 0x0600)
  Also there are additional fields, if (_WIN32_IE >= 0x0400).
    but (_WIN32_IE >= 0x0400) is expected.
  note:
  in x64 (64-bit):
  {
    (108 == REBARBANDINFO_V6_SIZE)
    (112 == sizeof(REBARBANDINFO) // for (_WIN32_WINNT <  0x0600)
    (128 == sizeof(REBARBANDINFO) // for (_WIN32_WINNT >= 0x0600)
    there is difference in sizes, because REBARBANDINFO size was
    not aligned for 8-bytes in (_WIN32_WINNT < 0x0600).
    We hope that WinVista+ support support both (108 and 112) sizes.
    But does WinXP-x64 support (108 == REBARBANDINFO_V6_SIZE)?
    {
         96   LPARAM  lParam;
        104   UINT    cxHeader;
      #if (_WIN32_WINNT >= 0x0600)
        108   RECT    rcChevronLocation;
        124   UINT    uChevronState;
      #endif
    }
*/

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600) && defined(REBARBANDINFOA_V6_SIZE)
  #define my_compatib_REBARBANDINFO_size  REBARBANDINFO_V6_SIZE
#else
  #define my_compatib_REBARBANDINFO_size  sizeof(REBARBANDINFO)
#endif


bool CPanel::OnCreate(CREATESTRUCT * /* createStruct */)
{
  // _virtualMode = false;
  // _sortIndex = 0;
  _sortID = kpidName;
  _ascending = true;
  _lastFocusedIsList = true;

  DWORD style = WS_CHILD | WS_VISIBLE; //  | WS_BORDER ; // | LVS_SHAREIMAGELISTS; //  | LVS_SHOWSELALWAYS;

  style |= LVS_SHAREIMAGELISTS;
  // style  |= LVS_AUTOARRANGE;
  style |= WS_CLIPCHILDREN;
  style |= WS_CLIPSIBLINGS;

  const UInt32 kNumListModes = Z7_ARRAY_SIZE(kStyles);
  if (_listViewMode >= kNumListModes)
    _listViewMode = kNumListModes - 1;

  style |= kStyles[_listViewMode]
    | WS_TABSTOP
    | LVS_EDITLABELS;
  if (_mySelectMode)
    style |= LVS_SINGLESEL;

  /*
  if (_virtualMode)
    style |= LVS_OWNERDATA;
  */

  DWORD exStyle;
  exStyle = WS_EX_CLIENTEDGE;

  if (!_listView.CreateEx(exStyle, style, 0, 0, 116, 260,
      *this, (HMENU)(UINT_PTR)(_baseID + 1), g_hInstance, NULL))
    return false;

  _listView.SetUnicodeFormat();
  _listView._panel = this;
  _listView.SetWindowProc();

  _listView.SetImageList(Shell_Get_SysImageList_smallIcons(true), LVSIL_SMALL);
  _listView.SetImageList(Shell_Get_SysImageList_smallIcons(false), LVSIL_NORMAL);

  // _exStyle |= LVS_EX_HEADERDRAGDROP;
  // DWORD extendedStyle = _listView.GetExtendedListViewStyle();
  // extendedStyle |= _exStyle;
  //  _listView.SetExtendedListViewStyle(extendedStyle);
  SetExtendedStyle();

  _listView.Show(SW_SHOW);
  _listView.InvalidateRect(NULL, true);
  _listView.Update();
  
  // Ensure that the common control DLL is loaded.
  INITCOMMONCONTROLSEX icex;

  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC  = ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  const TBBUTTON tbb[] =
  {
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    {VIEW_PARENTFOLDER, kParentFolderID, TBSTATE_ENABLED, BTNS_BUTTON, { 0, 0 }, 0, 0 },
    // {0, 0, TBSTATE_ENABLED, BTNS_SEP, 0L, 0},
    // {VIEW_NEWFOLDER, kCreateFolderID, TBSTATE_ENABLED, BTNS_BUTTON, 0L, 0},
  };

#ifdef Z7_USE_DYN_ComCtl32Version
  if (g_ComCtl32Version >= MAKELONG(71, 4))
#endif
  {
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC  = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // if there is no CCS_NOPARENTALIGN, there is space of some pixels after rebar (Incorrect GetWindowRect ?)

    _headerReBar.Attach(::CreateWindowEx(WS_EX_TOOLWINDOW,
      REBARCLASSNAME,
      NULL, WS_VISIBLE | WS_BORDER | WS_CHILD |
      WS_CLIPCHILDREN | WS_CLIPSIBLINGS
      | CCS_NODIVIDER
      | CCS_NOPARENTALIGN
      | CCS_TOP
      | RBS_VARHEIGHT
      | RBS_BANDBORDERS
      ,0,0,0,0, *this, NULL, g_hInstance, NULL));
  }

  DWORD toolbarStyle =  WS_CHILD | WS_VISIBLE ;
  if (_headerReBar)
  {
    toolbarStyle |= 0
      // | WS_CLIPCHILDREN
      // | WS_CLIPSIBLINGS

      | TBSTYLE_TOOLTIPS
      | CCS_NODIVIDER
      | CCS_NORESIZE
      | TBSTYLE_FLAT
      ;
  }

  _headerToolBar.Attach(::CreateToolbarEx ((*this), toolbarStyle,
      _baseID + 2, 11,
      (HINSTANCE)HINST_COMMCTRL,
      IDB_VIEW_SMALL_COLOR,
      (LPCTBBUTTON)&tbb, Z7_ARRAY_SIZE(tbb),
      0, 0, 0, 0, sizeof (TBBUTTON)));

  #ifndef UNDER_CE
  // Load ComboBoxEx class
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_USEREX_CLASSES;
  InitCommonControlsEx(&icex);
  #endif
  
  _headerComboBox.CreateEx(0,
      #ifdef UNDER_CE
      WC_COMBOBOXW
      #else
      WC_COMBOBOXEXW
      #endif
      , NULL,
    WS_BORDER | WS_VISIBLE |WS_CHILD | CBS_DROPDOWN | CBS_AUTOHSCROLL,
      0, 0, 100, 620,
      (_headerReBar ? _headerToolBar : (HWND)*this),
      (HMENU)(UINT_PTR)(_comboBoxID),
      g_hInstance, NULL);

#ifndef UNDER_CE
  _headerComboBox.SetUnicodeFormat(true);
  _headerComboBox.SetImageList(Shell_Get_SysImageList_smallIcons(true));
  _headerComboBox.SetExtendedStyle(CBES_EX_PATHWORDBREAKPROC, CBES_EX_PATHWORDBREAKPROC);
  /*
  _headerComboBox.SetUserDataLongPtr(LONG_PTR(&_headerComboBox));
  _headerComboBox._panel = this;
  _headerComboBox._origWindowProc =
      (WNDPROC)_headerComboBox.SetLongPtr(GWLP_WNDPROC,
      LONG_PTR(ComboBoxSubclassProc));
  */
  _comboBoxEdit.Attach(_headerComboBox.GetEditControl());
  // _comboBoxEdit.SendMessage(CCM_SETUNICODEFORMAT, (WPARAM)(BOOL)TRUE, 0);
  _comboBoxEdit.SetUserDataLongPtr(LONG_PTR(&_comboBoxEdit));
  _comboBoxEdit._panel = this;
   #ifndef _UNICODE
   if (g_IsNT)
     _comboBoxEdit._origWindowProc =
      (WNDPROC)_comboBoxEdit.SetLongPtrW(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));
   else
   #endif
     _comboBoxEdit._origWindowProc =
      (WNDPROC)_comboBoxEdit.SetLongPtr(GWLP_WNDPROC, LONG_PTR(ComboBoxEditSubclassProc));
#endif

  SetBigUpButtonImage();

  if (_headerReBar)
  {
    REBARINFO     rbi;
    rbi.cbSize = sizeof(REBARINFO);  // Required when using this struct.
    rbi.fMask  = 0;
    rbi.himl   = (HIMAGELIST)NULL;
    _headerReBar.SetBarInfo(&rbi);
    
    // Send the TB_BUTTONSTRUCTSIZE message, which is required for
    // backward compatibility.
    // _headerToolBar.SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SIZE size;
    _headerToolBar.GetMaxSize(&size);
    
    REBARBANDINFO rbBand;
    memset(&rbBand, 0, sizeof(rbBand));
    // rbBand.cbSize = sizeof(rbBand);  // for debug
    // rbBand.cbSize = REBARBANDINFO_V3_SIZE; // for debug
    rbBand.cbSize = my_compatib_REBARBANDINFO_size;
    rbBand.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
    rbBand.fStyle = RBBS_NOGRIPPER;
    rbBand.cxMinChild = (UINT)size.cx;
    rbBand.cyMinChild = (UINT)size.cy;
    rbBand.cyChild = (UINT)size.cy;
    rbBand.cx = (UINT)size.cx;
    rbBand.hwndChild  = _headerToolBar;
    _headerReBar.InsertBand(-1, &rbBand);

    RECT rc;
    ::GetWindowRect(_headerComboBox, &rc);
    rbBand.cxMinChild = 30;
    rbBand.cyMinChild = (UINT)(rc.bottom - rc.top);
    rbBand.cx = 1000;
    rbBand.hwndChild  = _headerComboBox;
    _headerReBar.InsertBand(-1, &rbBand);
    // _headerReBar.MaximizeBand(1, false);
  }

  _statusBar.Create(WS_CHILD | WS_VISIBLE, L"Status", (*this), _statusBarID);
  // _statusBar2.Create(WS_CHILD | WS_VISIBLE, L"Status", (*this), _statusBarID + 1);

  const int sizes[] = {220, 320, 420, -1};
  _statusBar.SetParts(4, sizes);
  _statusBar.SetStyle(_statusBar.GetStyle() | WS_CLIPSIBLINGS);

  // "Details" / "Large Icons" buttons (over the right end of status bar)
  {
    const HWND b1 = ::CreateWindowExW(0, L"BUTTON", L"Details",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 1, 1, *this, (HMENU)(UINT_PTR)kViewBtnID_Details, g_hInstance, NULL);
    const HWND b2 = ::CreateWindowExW(0, L"BUTTON", L"Large Icons",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        0, 0, 1, 1, *this, (HMENU)(UINT_PTR)kViewBtnID_Icons, g_hInstance, NULL);
    if (b1)
      ::SetWindowPos(b1, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (b2)
      ::SetWindowPos(b2, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  // _statusBar2.SetParts(5, sizes);

  /*
  RECT rect;
  GetClientRect(&rect);
  OnSize(0, RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
  */

  SetTimer(kTimerID, kTimerElapse);

  // InitListCtrl();
  RefreshListCtrl();
  
  return true;
}

/*
  big "up folder" button (like in WinRAR):
  the image size is about the height of the path box (it depends on DPI and font).
  Images: up16.bmp ... up64.bmp (32-bit with alpha).
  The image list is shared by both panels, and it lives until the program exits.
*/

static HIMAGELIST g_UpImageList;
static int g_UpImageSize;

void CPanel::SetBigUpButtonImage()
{
  if (!_headerToolBar)
    return;

  int want = 24;
  {
    RECT rc;
    if (_headerComboBox && ::GetWindowRect(_headerComboBox, &rc) && rc.bottom - rc.top > 8)
      want = (int)(rc.bottom - rc.top) - 2;
  }

  static const int kSizes[] = { 16, 20, 24, 28, 32, 40, 48, 56, 64 };
  static const UINT kIDs[] = { IDB_UP16, IDB_UP20, IDB_UP24, IDB_UP28, IDB_UP32,
      IDB_UP40, IDB_UP48, IDB_UP56, IDB_UP64 };
  unsigned best = 0;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kSizes); i++)
    if (kSizes[i] <= want)
      best = i;
  const int size = kSizes[best];

  if (!g_UpImageList || g_UpImageSize != size)
  {
    HIMAGELIST il = ImageList_Create(size, size, ILC_COLOR32, 1, 0);
    if (!il)
      return;
    HBITMAP b = (HBITMAP)::LoadImage(g_hInstance, MAKEINTRESOURCE(kIDs[best]),
        IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    if (!b)
    {
      ImageList_Destroy(il);
      return;
    }
    ImageList_Add(il, b, NULL);
    ::DeleteObject(b);
    g_UpImageList = il; // old list (if any) can still be used by other panel
    g_UpImageSize = size;
  }

  ::SendMessage(_headerToolBar, TB_SETIMAGELIST, 0, (LPARAM)g_UpImageList);
  ::SendMessage(_headerToolBar, TB_CHANGEBITMAP, (WPARAM)kParentFolderID, MAKELPARAM(0, 0));
  ::SendMessage(_headerToolBar, TB_SETPADDING, 0, MAKELPARAM(4, 4));
  ::SendMessage(_headerToolBar, TB_SETBUTTONSIZE, 0, MAKELPARAM(size + 4, size + 4));
  ::SendMessage(_headerToolBar, TB_AUTOSIZE, 0, 0);
}

void CPanel::ApplyTheme()
{
  if (!(HWND)*this)
    return;
  NDarkMode::ApplyToListView(_listView);
  NDarkMode::ApplyToComboBoxEx(_headerComboBox);
  NDarkMode::ApplyToStatusBar(_statusBar);
  NDarkMode::ApplyToBkWindow(*this);
  if (_headerReBar)
    NDarkMode::ApplyToBkWindow(_headerReBar);
  if (_headerToolBar)
    NDarkMode::ApplyToBkWindow(_headerToolBar);
}

// ---------- file list zoom (View / Zoom In, Zoom Out; Ctrl + mouse wheel) ----------

static const unsigned k_ZoomLevels[] = { 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400 };
static const unsigned k_ZoomDefault = 100;
static const wchar_t * const k_Reg_FM_Zoom = L"Software\\7-Zip\\FM";
static const wchar_t * const k_Reg_ListZoom = L"ListZoom";

static unsigned g_ListZoom = 0; // 0 : not loaded yet

static unsigned ZoomLevelIndex(unsigned zoom)
{
  // index of nearest level (that is not bigger than zoom)
  unsigned index = 0;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(k_ZoomLevels); i++)
    if (k_ZoomLevels[i] <= zoom)
      index = i;
  return index;
}

int GetListZoom()
{
  if (g_ListZoom == 0)
  {
    g_ListZoom = k_ZoomDefault;
    HKEY key;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, k_Reg_FM_Zoom, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
      DWORD value = 0;
      DWORD type = 0;
      DWORD size = sizeof(value);
      if (::RegQueryValueExW(key, k_Reg_ListZoom, NULL, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS
          && type == REG_DWORD)
        g_ListZoom = k_ZoomLevels[ZoomLevelIndex((unsigned)value)];
      ::RegCloseKey(key);
    }
  }
  return (int)g_ListZoom;
}

bool ChangeListZoom(int dir)
{
  const unsigned old = (unsigned)GetListZoom();
  unsigned index = ZoomLevelIndex(old);
  if (dir > 0)
  {
    if (index + 1 < Z7_ARRAY_SIZE(k_ZoomLevels))
      index++;
  }
  else if (dir < 0)
  {
    if (index > 0)
      index--;
  }
  else
    index = ZoomLevelIndex(k_ZoomDefault);
  g_ListZoom = k_ZoomLevels[index];
  if (g_ListZoom == old)
    return false;
  HKEY key;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, k_Reg_FM_Zoom, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS)
  {
    const DWORD value = g_ListZoom;
    ::RegSetValueExW(key, k_Reg_ListZoom, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
    ::RegCloseKey(key);
  }
  return true;
}

// shell image lists: SHIL_LARGE (32), SHIL_SMALL (16), SHIL_EXTRALARGE (48)
static HIMAGELIST GetShellImageList(int shil)
{
  typedef HRESULT (WINAPI *Func_SHGetImageList)(int iImageList, REFIID riid, void **ppv);
  static Func_SHGetImageList func = NULL;
  static bool loaded = false;
  if (!loaded)
  {
    loaded = true;
    const HMODULE shell = ::GetModuleHandleW(L"shell32.dll");
    if (shell)
    {
      func = Z7_GET_PROC_ADDRESS(Func_SHGetImageList, shell, "SHGetImageList");
      if (!func) // old Windows: exported only by ordinal 727
        func = Z7_GET_PROC_ADDRESS(Func_SHGetImageList, shell, (LPCSTR)(UINT_PTR)727);
    }
  }
  if (!func)
    return NULL;
  // IID_IImageList {46EB5926-582E-4017-9FDF-E8998DAA0950}
  static const GUID k_IID_IImageList =
    { 0x46EB5926, 0x582E, 0x4017, { 0x9F, 0xDF, 0xE8, 0x99, 0x8D, 0xAA, 0x09, 0x50 } };
  void *list = NULL;
  if (func(shil, k_IID_IImageList, &list) != S_OK)
    return NULL;
  return (HIMAGELIST)list; // IImageList can be used as HIMAGELIST
}

// fonts for zoom levels (they are shared by both panels and they are not deleted)
static HFONT g_ZoomFonts[Z7_ARRAY_SIZE(k_ZoomLevels)];

// font of list view for 100%: original font of list view or Windows icon font
static void GetListBaseFont(HFONT origFont, LOGFONTW &lf)
{
  memset(&lf, 0, sizeof(lf));
  if (origFont && ::GetObjectW(origFont, sizeof(lf), &lf) == sizeof(lf))
    return;
  memset(&lf, 0, sizeof(lf));
  if (!::SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0))
    ::GetObjectW(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
}

void CPanel::ApplyListZoom()
{
  if (!_listView)
    return;
  const unsigned zoom = (unsigned)GetListZoom();

  // font
  // we copy the font description of list view at first call (before any zoom).
  // We don't keep original HFONT: list view deletes its own font, when we set another font.
  static bool s_BaseFontDefined = false;
  static LOGFONTW s_BaseFont;
  if (!s_BaseFontDefined)
  {
    s_BaseFontDefined = true;
    GetListBaseFont((HFONT)_listView.SendMsg(WM_GETFONT, 0, 0), s_BaseFont);
  }
  HFONT font;
  {
    const unsigned index = ZoomLevelIndex(zoom);
    if (!g_ZoomFonts[index])
    {
      LOGFONTW lf = s_BaseFont;
      lf.lfHeight = ::MulDiv(lf.lfHeight, (int)k_ZoomLevels[index], 100);
      g_ZoomFonts[index] = ::CreateFontIndirectW(&lf);
    }
    font = g_ZoomFonts[index];
  }
  _listView.SendMsg(WM_SETFONT, (WPARAM)font, FALSE);
  {
    // header of list view: same font
    const HWND header = ListView_GetHeader(_listView);
    if (header)
      ::SendMessageW(header, WM_SETFONT, (WPARAM)font, TRUE);
  }

  // icons
  HIMAGELIST smallList = NULL;
  HIMAGELIST largeList = NULL;
  if (zoom >= 250)
  {
    smallList = GetShellImageList(2); // SHIL_EXTRALARGE: 48
    largeList = GetShellImageList(2);
  }
  else if (zoom >= 150)
  {
    smallList = GetShellImageList(0); // SHIL_LARGE: 32
    largeList = GetShellImageList(2); // SHIL_EXTRALARGE: 48
  }
  if (!smallList)
    smallList = Shell_Get_SysImageList_smallIcons(true);
  if (!largeList)
    largeList = Shell_Get_SysImageList_smallIcons(false);
  _listView.SetImageList(smallList, LVSIL_SMALL);
  _listView.SetImageList(largeList, LVSIL_NORMAL);

  // new row height / item size
  const LONG_PTR style = _listView.GetStyle();
  if ((style & LVS_TYPEMASK) == LVS_ICON)
    _listView.SendMsg(LVM_ARRANGE, LVA_DEFAULT, 0);
  _listView.InvalidateRect(NULL, true);
}

void CPanel::OnDestroy()
{
  SaveListViewInfo();
  CWindow2::OnDestroy();
}

void CPanel::ChangeWindowSize(int xSize, int ySize)
{
  if (!(HWND)*this)
    return;
  int kHeaderSize;
  int kStatusBarSize;
  // int kStatusBar2Size;
  RECT rect;
  if (_headerReBar)
    _headerReBar.GetWindowRect(&rect);
  else
    _headerToolBar.GetWindowRect(&rect);

  kHeaderSize = RECT_SIZE_Y(rect);

  _statusBar.GetWindowRect(&rect);
  kStatusBarSize = RECT_SIZE_Y(rect);
  
  // _statusBar2.GetWindowRect(&rect);
  // kStatusBar2Size = RECT_SIZE_Y(rect);
 
  int yListViewSize = MyMax(ySize - kHeaderSize - kStatusBarSize, 0);
  const int kStartXPos = 32;
  if (_headerReBar)
  {
  }
  else
  {
    _headerToolBar.Move(0, 0, xSize, 0);
    _headerComboBox.Move(kStartXPos, 2,
        MyMax(xSize - kStartXPos - 10, kStartXPos), 0);
  }
  _listView.Move(0, kHeaderSize, xSize, yListViewSize);
  _statusBar.Move(0, kHeaderSize + yListViewSize, xSize, kStatusBarSize);
  {
    // "Details" / "Large Icons" buttons at the right end of status bar
    const int bh = kStatusBarSize > 6 ? kStatusBarSize - 2 : kStatusBarSize;
    const int bw = bh + bh / 4;
    const int gripW = 4; // small space at right side
    const int x2 = xSize - gripW - bw;
    const int x1 = x2 - bw - 2;
    const int y = kHeaderSize + yListViewSize + (kStatusBarSize - bh) / 2;
    const HWND b1 = ::GetDlgItem(*this, kViewBtnID_Details);
    const HWND b2 = ::GetDlgItem(*this, kViewBtnID_Icons);
    // SWP_NOCOPYBITS: buttons are drawn again at new place (no old copied pixels)
    if (b1)
      ::SetWindowPos(b1, HWND_TOP, x1, y, bw, bh, SWP_NOACTIVATE | SWP_NOCOPYBITS);
    if (b2)
      ::SetWindowPos(b2, HWND_TOP, x2, y, bw, bh, SWP_NOACTIVATE | SWP_NOCOPYBITS);
    if (b1) ::InvalidateRect(b1, NULL, FALSE);
    if (b2) ::InvalidateRect(b2, NULL, FALSE);
    // last part of status bar ends before the buttons
    int last = x1 - 4;
    if (last < 430)
      last = 430;
    const int sizes[] = { 220, 320, 420, last };
    _statusBar.SetParts(4, sizes);
    // status bar: remove old pictures of buttons from previous place
    _statusBar.InvalidateRect(NULL, true);
  }
  // _statusBar2.MoveWindow(0, kHeaderSize + yListViewSize + kStatusBarSize, xSize, kStatusBar2Size);
  // _statusBar.MoveWindow(0, 100, xSize, kStatusBarSize);
  // _statusBar2.MoveWindow(0, 200, xSize, kStatusBar2Size);
}

bool CPanel::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  if (!(HWND)*this)
    return true;
  if (_headerReBar)
    _headerReBar.Move(0, 0, xSize, 0);
  ChangeWindowSize(xSize, ySize);
  return true;
}

bool CPanel::OnNotifyReBar(LPNMHDR header, LRESULT & /* result */)
{
  switch (header->code)
  {
    case RBN_HEIGHTCHANGE:
    {
      RECT rect;
      GetWindowRect(&rect);
      ChangeWindowSize(RECT_SIZE_X(rect), RECT_SIZE_Y(rect));
      return false;
    }
  }
  return false;
}

/*
UInt32 g_OnNotify = 0;
UInt32 g_LVIF_TEXT = 0;
UInt32 g_Time = 0;

void Print_OnNotify(const char *name)
{
  char s[256];
  DWORD tim = GetTickCount();
  sprintf(s,
      "Time = %7u ms, Notify = %9u, TEXT = %9u, %s",
      tim - g_Time,
      g_OnNotify,
      g_LVIF_TEXT,
      name);
  g_Time = tim;
  OutputDebugStringA(s);
  g_OnNotify = 0;
  g_LVIF_TEXT = 0;
}
*/

bool CPanel::OnNotify(UINT /* controlID */, LPNMHDR header, LRESULT &result)
{
  /*
  g_OnNotify++;

  if (header->hwndFrom == _listView)
  {
    if (header->code == LVN_GETDISPINFOW)
    {
      LV_DISPINFOW *dispInfo = (LV_DISPINFOW *)header;
        if ((dispInfo->item.mask & LVIF_TEXT))
          g_LVIF_TEXT++;
    }
  }
  */

  if (!_processNotify)
    return false;

  if (header->hwndFrom == _headerComboBox)
    return OnNotifyComboBox(header, result);
  else if (header->hwndFrom == _headerReBar)
    return OnNotifyReBar(header, result);
  else if (header->hwndFrom == _listView)
    return OnNotifyList(header, result);
  else if (::GetParent(header->hwndFrom) == _listView)
  {
    // NMHDR:code is UINT
    // NM_RCLICK is unsigned in windows sdk
    // NM_RCLICK is int      in MinGW
    if (header->code == (UINT)NM_RCLICK)
      return OnRightClick((MY_NMLISTVIEW_NMITEMACTIVATE *)header, result);
  }
  return false;
}

bool CPanel::OnCommand(unsigned code, unsigned itemID, LPARAM lParam, LRESULT &result)
{
  if (itemID == kParentFolderID)
  {
    OpenParentFolder();
    result = 0;
    return true;
  }
  /*
  if (itemID == kCreateFolderID)
  {
    CreateFolder();
    result = 0;
    return true;
  }
  */
  if (itemID == _comboBoxID)
  {
    if (OnComboBoxCommand(code, lParam, result))
      return true;
  }
  if (itemID == (unsigned)kViewBtnID_Details || itemID == (unsigned)kViewBtnID_Icons)
  {
    SetListViewMode(itemID == (unsigned)kViewBtnID_Details ? kViewMode_Details : kViewMode_Icons);
    _listView.SetFocus();
    result = 0;
    return true;
  }
  return CWindow2::OnCommand(code, itemID, lParam, result);
}



/*
void CPanel::MessageBox_Info(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK); }
void CPanel::MessageBox_Warning(LPCWSTR message) const
  { ::MessageBoxW((HWND)*this, message, L"7-Zip", MB_OK | MB_ICONWARNING); }
*/

void CPanel::MessageBox_Error_Caption(LPCWSTR message, LPCWSTR caption) const
  { ::MessageBoxW((HWND)*this, message, caption, MB_OK | MB_ICONSTOP); }

void CPanel::MessageBox_Error(LPCWSTR message) const
  { MessageBox_Error_Caption(message, L"7-Zip"); }

static UString ErrorHResult_To_Message(HRESULT errorCode)
{
  if (errorCode == 0)
    errorCode = E_FAIL;
  return HResultToMessage(errorCode);
}

void CPanel::MessageBox_Error_HRESULT_Caption(HRESULT errorCode, LPCWSTR caption) const
{
  MessageBox_Error_Caption(ErrorHResult_To_Message(errorCode), caption);
}

void CPanel::MessageBox_Error_HRESULT(HRESULT errorCode) const
  { MessageBox_Error_HRESULT_Caption(errorCode, L"7-Zip"); }

void CPanel::MessageBox_Error_2Lines_Message_HRESULT(LPCWSTR message, HRESULT errorCode) const
{
  UString m = message;
  m.Add_LF();
  m += ErrorHResult_To_Message(errorCode);
  MessageBox_Error(m);
}

void CPanel::MessageBox_LastError(LPCWSTR caption) const
  { MessageBox_Error_HRESULT_Caption(GetLastError_noZero_HRESULT(), caption); }

void CPanel::MessageBox_LastError() const
  { MessageBox_LastError(L"7-Zip"); }

void CPanel::MessageBox_Error_LangID(UINT resourceID) const
  { MessageBox_Error(LangString(resourceID)); }

void CPanel::MessageBox_Error_UnsupportOperation() const
  { MessageBox_Error_LangID(IDS_OPERATION_IS_NOT_SUPPORTED); }




void CPanel::SetFocusToList()
{
  _listView.SetFocus();
  // SetCurrentPathText();
}

void CPanel::SetFocusToLastRememberedItem()
{
  if (_lastFocusedIsList)
    SetFocusToList();
  else
    _headerComboBox.SetFocus();
}

UString CPanel::GetFolderTypeID() const
{
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidType, &prop) == S_OK)
      if (prop.vt == VT_BSTR)
        return (const wchar_t *)prop.bstrVal;
  }
  return UString();
}

bool CPanel::IsFolderTypeEqTo(const char *s) const
{
  return StringsAreEqual_Ascii(GetFolderTypeID(), s);
}

bool CPanel::IsRootFolder() const { return IsFolderTypeEqTo("RootFolder"); }
bool CPanel::IsFSFolder() const { return IsFolderTypeEqTo("FSFolder"); }
bool CPanel::IsFSDrivesFolder() const { return IsFolderTypeEqTo("FSDrives"); }
bool CPanel::IsAltStreamsFolder() const { return IsFolderTypeEqTo("AltStreamsFolder"); }
bool CPanel::IsArcFolder() const
{
  return GetFolderTypeID().IsPrefixedBy_Ascii_NoCase("7-Zip");
}

bool CPanel::IsHashFolder() const
{
  if (_folder)
  {
    NCOM::CPropVariant prop;
    if (_folder->GetFolderProperty(kpidIsHash, &prop) == S_OK)
      if (prop.vt == VT_BOOL)
        return VARIANT_BOOLToBool(prop.boolVal);
  }
  return false;
}

UString CPanel::GetFsPath() const
{
  if (IsFSDrivesFolder() && !IsDeviceDrivesPrefix() && !IsSuperDrivesPrefix())
    return UString();
  return _currentFolderPrefix;
}

UString CPanel::GetDriveOrNetworkPrefix() const
{
  if (!IsFSFolder())
    return UString();
  UString drive = GetFsPath();
  drive.DeleteFrom(NFile::NName::GetRootPrefixSize(drive));
  return drive;
}

void CPanel::SetListViewMode(UInt32 index)
{
  if (index >= 4)
    return;
  _listViewMode = index;
  const LONG_PTR oldStyle = _listView.GetStyle();
  const DWORD newStyle = kStyles[index];

  // DWORD tickCount1 = GetTickCount();
  if ((oldStyle & LVS_TYPEMASK) != (LONG_PTR)newStyle)
    _listView.SetStyle((oldStyle & ~(LONG_PTR)(DWORD)LVS_TYPEMASK) | (LONG_PTR)newStyle);
  {
    // "Details" / "Large Icons" buttons show current view
    const HWND b1 = ::GetDlgItem(*this, kViewBtnID_Details);
    const HWND b2 = ::GetDlgItem(*this, kViewBtnID_Icons);
    if (b1) ::InvalidateRect(b1, NULL, FALSE);
    if (b2) ::InvalidateRect(b2, NULL, FALSE);
  }
  // RefreshListCtrlSaveFocused();
  /*
  DWORD tickCount2 = GetTickCount();
  char s[256];
  sprintf(s, "SetStyle = %5d",
      tickCount2 - tickCount1
      );
  OutputDebugStringA(s);
  */

}

void CPanel::ChangeFlatMode()
{
  _flatMode = !_flatMode;
  if (!_parentFolders.IsEmpty())
    _flatModeForArc = _flatMode;
  else
    _flatModeForDisk = _flatMode;
  RefreshListCtrl_SaveFocused();
}

/*
void CPanel::Change_ShowNtfsStrems_Mode()
{
  _showNtfsStrems_Mode = !_showNtfsStrems_Mode;
  if (!_parentFolders.IsEmpty())
    _showNtfsStrems_ModeForArc = _showNtfsStrems_Mode;
  else
    _showNtfsStrems_ModeForDisk = _showNtfsStrems_Mode;
  RefreshListCtrlSaveFocused();
}
*/

void CPanel::Post_Refresh_StatusBar()
{
  if (_processStatusBar)
    PostMsg(kRefresh_StatusBar);
}

void CPanel::AddToArchive()
{
  if (!Is_IO_FS_Folder())
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.Size() == 0)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UString destCurDirPrefix = GetFsPath();
  if (IsFSDrivesFolder())
    destCurDirPrefix = ROOT_FS_FOLDER;
  UStringVector names;
  GetFilePaths(indices, names);
  UString baseName;
  const UString arcName = CreateArchiveName(names,
      false, // isHash
      NULL,  // CFileInfo *fi
      baseName);
  const HRESULT res = CompressFiles(destCurDirPrefix, arcName, L"",
      true,   // addExtension
      names,
      false,  // email
      true,   // showDialog
      false); // waitFinish
  if (res != S_OK)
  {
    if (destCurDirPrefix.Len() >= MAX_PATH)
      MessageBox_Error_LangID(IDS_MESSAGE_UNSUPPORTED_OPERATION_FOR_LONG_PATH_FOLDER);
  }
  // KillSelection();
}

// function from ContextMenu.cpp
UString GetSubFolderNameForExtract(const UString &arcPath);

static UString GetSubFolderNameForExtract2(const UString &arcPath)
{
  int slashPos = arcPath.ReverseFind_PathSepar();
  UString s;
  UString name = arcPath;
  if (slashPos >= 0)
  {
    s = arcPath.Left((unsigned)(slashPos + 1));
    name = arcPath.Ptr((unsigned)(slashPos + 1));
  }
  s += GetSubFolderNameForExtract(name);
  return s;
}


void OpenFolderInExplorer(const UString &folder); // App.cpp
extern bool g_EasyExtract_OpenAfter; // App.cpp

/*
  "Easy Extract" (toolbar button, Tools menu):
  like "Extract to "ArchiveName\"" in Explorer context menu:
  each selected archive is extracted (without questions) to the folder
  with archive name, next to the archive.
  When it's done, the extracted folder is opened in Windows File Explorer.
*/
void CPanel::EasyExtract()
{
  if (!IsFSFolder())
  {
    // inside archive: Easy Extract always extracts the whole archive
    // (all files, also if some files are selected)
    if (!_parentFolders.IsEmpty())
    {
      const UString &arcPath = _parentFolders[0].VirtualPath;
      const DWORD attrib = ::GetFileAttributesW(arcPath);
      if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) == 0)
      {
        UStringVector paths;
        paths.Add(arcPath);
        EasyExtractFiles(paths);
        return;
      }
    }
    // archive inside other archive: extract all items of current folder
    SelectAll(true);
    g_App.OnCopy(false, false, g_App.GetFocusedPanelIndex(), true);
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.IsEmpty() || FindDir_InOperatedList(indices) != -1)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  EasyExtractFiles(paths);
}

static UString GetFileNameOfPath(const UString &path)
{
  return UString(path.Ptr((unsigned)(path.ReverseFind_PathSepar() + 1)));
}

// "Easy Extract" for archive files (full paths): dialog, extract, open folders
void CPanel::EasyExtractFiles(const UStringVector &paths)
{
  if (paths.IsEmpty())
    return;
  // folder of archives
  UString fsPath = paths[0];
  fsPath.DeleteFrom((unsigned)(fsPath.ReverseFind_PathSepar() + 1));

  UString outFolder = fsPath;
  if (paths.Size() == 1)
    outFolder += GetSubFolderNameForExtract(GetFileNameOfPath(paths[0]));
  else
    outFolder.Add_Char('*'); // 7-Zip replaces '*' with name of each archive
  outFolder.Add_PathSepar();

  // "Easy Extract" dialog: user can change the folder and "open when done" option
  bool openAfter = true;
  {
    CCopyDialog dialog;
    dialog.Value = outFolder;
    {
      // list of archives
      const unsigned kMaxLines = 8;
      FOR_VECTOR (i, paths)
      {
        if (i == kMaxLines)
        {
          dialog.Info += "...";
          break;
        }
        dialog.Info += GetFileNameOfPath(paths[i]);
        dialog.Info.Add_LF();
      }
    }
    if (outFolder.Find(L'*') >= 0)
    {
      // explanation of '*' in "Extract to" path
      UString star;
      LangString(IDS_EASY_EXTRACT_STAR, star);
      star += "\n\n";
      dialog.Info.Insert(0, star);
    }
    LangString(IDS_EASY_EXTRACT, dialog.Title);
    LangString(IDS_EASY_EXTRACT_TO, dialog.Static);
    LangString(IDS_EASY_EXTRACT_OPEN, dialog.OpenOptionText);
    dialog.ShowOpenOption = true;
    dialog.IconResID = IDI_EASY_EXTRACT;
    dialog.OpenAfter = g_EasyExtract_OpenAfter;
    if (dialog.Create(GetParent()) != IDOK)
      return;
    g_EasyExtract_OpenAfter = dialog.OpenAfter;
    openAfter = dialog.OpenAfter;
    outFolder = dialog.Value;
    outFolder.Trim();
    if (outFolder.IsEmpty())
      return;
    if (!IS_PATH_SEPAR(outFolder.Back()))
      outFolder.Add_PathSepar();
    // relative path: from folder of archives
    if (!NWindows::NFile::NName::IsAbsolutePath(outFolder))
      outFolder.Insert(0, fsPath);
  }

  // folders that we open after extracting: '*' is replaced with name of each archive
  UStringVector openFolders;
  {
    const int star = outFolder.Find(L'*');
    FOR_VECTOR (i, paths)
    {
      UString folder = outFolder;
      if (star >= 0)
      {
        folder.Delete((unsigned)star);
        folder.Insert((unsigned)star, GetSubFolderNameForExtract(GetFileNameOfPath(paths[i])));
      }
      bool found = false;
      FOR_VECTOR (k, openFolders)
        if (openFolders[k].IsEqualTo_NoCase(folder))
        {
          found = true;
          break;
        }
      if (!found)
        openFolders.Add(folder);
    }
  }

  CContextMenuInfo ci;
  ci.Load();
  ::ExtractArchives(paths, outFolder
      , false  // showDialog: no questions
      , ci.ElimDup.Val // like "Extract to" in context menu: no duplicated root folder
      , ci.WriteZone
      );

  if (IsFSFolder())
    RefreshListCtrl();
  if (!openAfter)
    return;

  // open all extracted files folders in Windows File Explorer
  FOR_VECTOR (i, openFolders)
  {
    UString checkPath = openFolders[i];
    checkPath.DeleteBack();
    const DWORD attrib = ::GetFileAttributesW(checkPath);
    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0)
      OpenFolderInExplorer(openFolders[i]);
  }
}

int CPanel::FindDir_InOperatedList(const CRecordVector<UInt32> &operatedIndices) const
{
  const bool *isDirVector = _isDirVector.ConstData();
  const UInt32 *indices = operatedIndices.ConstData();
  const unsigned numItems = operatedIndices.Size();
  for (unsigned i = 0; i < numItems; i++)
    if (isDirVector[indices[i]])
      return (int)i;
  return -1;
}


void CPanel::GetFilePaths(const CRecordVector<UInt32> &operatedIndices, UStringVector &paths) const
{
  paths.ClearAndReserve(operatedIndices.Size());
  UString path = GetFsPath();
  const unsigned prefixLen = path.Len();
  const UInt32 *indices = operatedIndices.ConstData();
  const unsigned numItems = operatedIndices.Size();
  // for (unsigned y = 0; y < 10000; y++, paths.Clear())
  for (unsigned i = 0; i < numItems; i++)
  {
    path.DeleteFrom(prefixLen);
    Add_ItemRelPath2_To_String(indices[i], path);
    // ODS_U(path)
    paths.AddInReserved(path);
  }
}


void CPanel::ExtractArchives()
{
  if (!_parentFolders.IsEmpty())
  {
    _panelCallback->OnCopy(false, false);
    return;
  }
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.IsEmpty() || FindDir_InOperatedList(indices) != -1)
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  UString outFolder = GetFsPath();
  if (indices.Size() == 1)
    outFolder += GetSubFolderNameForExtract2(GetItemRelPath(indices[0]));
  else
    outFolder.Add_Char('*');
  outFolder.Add_PathSepar();
  
  CContextMenuInfo ci;
  ci.Load();

  ::ExtractArchives(paths, outFolder
      , true   // showDialog
      , false  // elimDup
      , ci.WriteZone
      );
}

/*
static void AddValuePair(UINT resourceID, UInt64 value, UString &s)
{
  AddLangString(s, resourceID);
  char sz[32];
  s += ": ";
  ConvertUInt64ToString(value, sz);
  s += sz;
  s.Add_LF();
}

// now we don't need CThreadTest, since now we call CopyTo for "test command

class CThreadTest: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  CRecordVector<UInt32> Indices;
  CExtractCallbackImp *ExtractCallbackSpec;
  CMyComPtr<IFolderArchiveExtractCallback> ExtractCallback;
  CMyComPtr<IArchiveFolder> ArchiveFolder;
};

HRESULT CThreadTest::ProcessVirt()
{
  RINOK(ArchiveFolder->Extract(&Indices[0], Indices.Size(),
      true, // includeAltStreams
      false, // replaceAltStreamColon
      NExtract::NPathMode::kFullPathnames,
      NExtract::NOverwriteMode::kAskBefore,
      NULL, // path
      BoolToInt(true), // testMode
      ExtractCallback));
  if (ExtractCallbackSpec->IsOK())
  {
    UString s;
    AddValuePair(IDS_PROP_FOLDERS, ExtractCallbackSpec->NumFolders, s);
    AddValuePair(IDS_PROP_FILES, ExtractCallbackSpec->NumFiles, s);
    // AddValuePair(IDS_PROP_SIZE, ExtractCallbackSpec->UnpackSize, s);
    // AddSizePair(IDS_COMPRESSED_COLON, Stat.PackSize, s);
    s.Add_LF();
    AddLangString(s, IDS_MESSAGE_NO_ERRORS);
    FinalMessage.OkMessage.Message = s;
  }
  return S_OK;
}

static void AddSizePair(UInt32 langID, UInt64 value, UString &s)
{
  char sz[32];
  AddLangString(s, langID);
  s += L' ';
  ConvertUInt64ToString(value, sz);
  s += sz;
  ConvertUInt64ToString(value >> 20, sz);
  s += " (";
  s += sz;
  s += " MB)";
  s.Add_LF();
}
*/

void CPanel::TestArchives()
{
  CRecordVector<UInt32> indices;
  Get_ItemIndices_OperSmart(indices);
  CMyComPtr<IArchiveFolder> archiveFolder;
  _folder.QueryInterface(IID_IArchiveFolder, &archiveFolder);
  if (archiveFolder)
  {
    CCopyToOptions options;
    options.streamMode = true;
    options.showErrorMessages = true;
    options.testMode = true;

    UStringVector messages;
    HRESULT res = CopyTo(options, indices, &messages);
    if (res != S_OK)
    {
      if (res != E_ABORT)
        MessageBox_Error_HRESULT(res);
    }
    return;

    /*
    {
    CThreadTest extracter;

    extracter.ArchiveFolder = archiveFolder;
    extracter.ExtractCallbackSpec = new CExtractCallbackImp;
    extracter.ExtractCallback = extracter.ExtractCallbackSpec;
    extracter.ExtractCallbackSpec->ProgressDialog = &extracter.ProgressDialog;
    if (!_parentFolders.IsEmpty())
    {
      const CFolderLink &fl = _parentFolders.Back();
      extracter.ExtractCallbackSpec->PasswordIsDefined = fl.UsePassword;
      extracter.ExtractCallbackSpec->Password = fl.Password;
    }

    if (indices.IsEmpty())
      return;

    extracter.Indices = indices;
    
    const UString title = LangString(IDS_PROGRESS_TESTING);
    
    extracter.ProgressDialog.CompressingMode = false;
    extracter.ProgressDialog.MainWindow = GetParent();
    extracter.ProgressDialog.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
    extracter.ProgressDialog.MainAddTitle = title + L' ';
    
    extracter.ExtractCallbackSpec->OverwriteMode = NExtract::NOverwriteMode::kAskBefore;
    extracter.ExtractCallbackSpec->Init();
    
    if (extracter.Create(title, GetParent()) != S_OK)
      return;
    
    }
    RefreshTitleAlways();
    return;
    */
  }

  if (!IsFSFolder())
  {
    MessageBox_Error_UnsupportOperation();
    return;
  }
  UStringVector paths;
  GetFilePaths(indices, paths);
  if (paths.IsEmpty())
  {
    MessageBox_Error_LangID(IDS_SELECT_FILES);
    return;
  }
  ::TestArchives(paths);
}
