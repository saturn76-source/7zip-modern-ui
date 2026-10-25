// ToolbarCustomizeDialog.cpp

#include "StdAfx.h"

#include "ToolbarCustomizeDialog.h"

#ifdef Z7_LANG
#include "LangUtils.h"
#endif

using namespace NWindows;

static const int kSeparator = -1;

static HCURSOR GetDragCursor();

static int GetSelected(NControl::CListView &list)
{
  return list.GetNextSelectedItem(-1);
}

void CToolbarCustomizeDialog::InitList(NControl::CListView &list)
{
  list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  if (ImageList)
    list.SetImageList(ImageList, LVSIL_SMALL);
  RECT rc;
  list.GetClientRect(&rc);
  list.InsertColumn(0, L"", (int)(rc.right - rc.left));
  // column fills the whole width of list (selection is full width)
  ListView_SetColumnWidth(list, 0, LVSCW_AUTOSIZE_USEHEADER);
}

// Move Up, Move Down, Reset: at the right of "Current" list, inside the window
void CToolbarCustomizeDialog::PlaceRightButtons()
{
  RECT client;
  if (!::GetClientRect(*this, &client))
    return;
  RECT listRect;
  GetClientRectOfItem(IDL_TBC_CURRENT, listRect);
  RECT okRect;
  GetClientRectOfItem(IDOK, okRect);
  int mx, my;
  GetMargins(8, mx, my);
  const int x = listRect.right + mx;
  int w = (int)client.right - mx - x;
  if (w < 20)
    w = 20;
  const int h = (int)(okRect.bottom - okRect.top);
  const int y0 = listRect.top;
  MoveItem(IDB_TBC_UP,    x, y0, w, h);
  MoveItem(IDB_TBC_DOWN,  x, y0 + h + my / 2, w, h);
  MoveItem(IDB_TBC_RESET, x, y0 + (h + my / 2) * 2 + my, w, h);
}

bool CToolbarCustomizeDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  if (SeparatorText.IsEmpty())
    SeparatorText = "Separator";
  _avail.Attach(GetItem(IDL_TBC_AVAIL));
  _current.Attach(GetItem(IDL_TBC_CURRENT));
  InitList(_avail);
  InitList(_current);
  PlaceRightButtons();
  FillLists(0, Layout.IsEmpty() ? -1 : 0);
  NormalizePosition();
  return CModalDialog::OnInit();
}

static void AddListItem(NControl::CListView &list, unsigned index, const UString &text, int image)
{
  LVITEMW item;
  memset(&item, 0, sizeof(item));
  item.mask = LVIF_TEXT | LVIF_IMAGE;
  item.iItem = (int)index;
  item.pszText = text.Ptr_non_const();
  item.iImage = (image >= 0) ? image : I_IMAGENONE;
  list.InsertItem(&item);
}

// fills both lists from Layout. Available: separator, and all buttons that are not in Layout
void CToolbarCustomizeDialog::FillLists(int availSel, int currentSel)
{
  _availItems.Clear();
  _availItems.Add(kSeparator);
  FOR_VECTOR (i, Buttons)
  {
    bool used = false;
    FOR_VECTOR (k, Layout)
      if (Layout[k] == (int)i)
      {
        used = true;
        break;
      }
    if (!used)
      _availItems.Add((int)i);
  }

  _avail.SetRedraw(false);
  _current.SetRedraw(false);
  _avail.DeleteAllItems();
  _current.DeleteAllItems();
  FOR_VECTOR (a, _availItems)
  {
    const int k = _availItems[a];
    if (k == kSeparator)
      AddListItem(_avail, a, SeparatorText, -1);
    else
      AddListItem(_avail, a, Buttons[(unsigned)k].Text, Buttons[(unsigned)k].ImageIndex);
  }
  FOR_VECTOR (c, Layout)
  {
    const int k = Layout[c];
    if (k < 0 || (unsigned)k >= Buttons.Size())
      AddListItem(_current, c, SeparatorText, -1);
    else
      AddListItem(_current, c, Buttons[(unsigned)k].Text, Buttons[(unsigned)k].ImageIndex);
  }
  _avail.SetRedraw(true);
  _current.SetRedraw(true);

  if (availSel >= (int)_availItems.Size())
    availSel = (int)_availItems.Size() - 1;
  if (currentSel >= (int)Layout.Size())
    currentSel = (int)Layout.Size() - 1;
  if (availSel >= 0)
  {
    _avail.SetItemState_FocusedSelected(availSel);
    _avail.EnsureVisible(availSel, false);
  }
  if (currentSel >= 0)
  {
    _current.SetItemState_FocusedSelected(currentSel);
    _current.EnsureVisible(currentSel, false);
  }
  ::InvalidateRect(_avail, NULL, TRUE);
  ::InvalidateRect(_current, NULL, TRUE);
  UpdateButtons();
}

void CToolbarCustomizeDialog::UpdateButtons()
{
  const int a = GetSelected(_avail);
  const int c = GetSelected(_current);
  EnableItem(IDB_TBC_ADD, a >= 0);
  EnableItem(IDB_TBC_REMOVE, c >= 0);
  EnableItem(IDB_TBC_UP, c > 0);
  EnableItem(IDB_TBC_DOWN, c >= 0 && c + 1 < (int)Layout.Size());
}

// adds selected available item after selected current item (or to the end)
void CToolbarCustomizeDialog::AddSelected()
{
  const int a = GetSelected(_avail);
  if (a < 0 || (unsigned)a >= _availItems.Size())
    return;
  int pos = GetSelected(_current);
  pos = (pos < 0) ? (int)Layout.Size() : pos + 1;
  Layout.Insert((unsigned)pos, _availItems[(unsigned)a]);
  FillLists(a, pos);
}

void CToolbarCustomizeDialog::RemoveSelected()
{
  const int c = GetSelected(_current);
  if (c < 0 || (unsigned)c >= Layout.Size())
    return;
  Layout.Delete((unsigned)c);
  FillLists(GetSelected(_avail), c);
}

void CToolbarCustomizeDialog::Move(bool up)
{
  const int c = GetSelected(_current);
  if (c < 0)
    return;
  const int n = up ? c - 1 : c + 1;
  if (n < 0 || n >= (int)Layout.Size())
    return;
  const int t = Layout[(unsigned)c];
  Layout[(unsigned)c] = Layout[(unsigned)n];
  Layout[(unsigned)n] = t;
  FillLists(GetSelected(_avail), n);
}

bool CToolbarCustomizeDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_TBC_ADD:    AddSelected(); return true;
    case IDB_TBC_REMOVE: RemoveSelected(); return true;
    case IDB_TBC_UP:     Move(true); return true;
    case IDB_TBC_DOWN:   Move(false); return true;
    case IDB_TBC_RESET:
      Layout = DefaultLayout;
      FillLists(0, Layout.IsEmpty() ? -1 : 0);
      return true;
    default:
      break;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

bool CToolbarCustomizeDialog::OnNotify(UINT controlID, LPNMHDR header)
{
  if (controlID != IDL_TBC_AVAIL && controlID != IDL_TBC_CURRENT)
    return false;
  switch (header->code)
  {
    case LVN_ITEMCHANGED:
      UpdateButtons();
      return false;
    case LVN_BEGINDRAG:
    {
      // start of drag and drop
      const NMLISTVIEW *nm = (const NMLISTVIEW *)header;
      _dragFromCurrent = (controlID == IDL_TBC_CURRENT);
      _dragIndex = nm->iItem;
      if (_dragIndex < 0)
        return true;
      _dragging = true;
      _dropHilite = -1;
      ::SetCapture(*this);
      ::SetCursor(GetDragCursor());
      return true;
    }
    case NM_DBLCLK:
      // double click: add / remove
      if (controlID == IDL_TBC_AVAIL)
        AddSelected();
      else
        RemoveSelected();
      return true;
    default:
      break;
  }
  return false;
}

extern HINSTANCE g_hInstance;

// "move" drag cursor: arrow with small box (DragMove.cur in resources)
static HCURSOR GetDragCursor()
{
  static HCURSOR cursor = NULL;
  if (!cursor)
    cursor = (HCURSOR)::LoadImageW(g_hInstance, MAKEINTRESOURCEW(IDC_TBC_DRAG_CURSOR),
        IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
  if (!cursor)
    cursor = ::LoadCursor(NULL, IDC_ARROW);
  return cursor;
}

// returns the list window under the point (dialog client coordinates), and the point in that list
static HWND GetListUnderPoint(HWND dialog, LPARAM lParam, HWND list1, HWND list2, POINT &ptList)
{
  POINT pt;
  pt.x = (short)LOWORD(lParam);
  pt.y = (short)HIWORD(lParam);
  ::ClientToScreen(dialog, &pt);
  HWND w = ::WindowFromPoint(pt);
  // the point can be over header or scroll bar: we look for parent list
  while (w && w != list1 && w != list2 && w != dialog)
    w = ::GetParent(w);
  if (w != list1 && w != list2)
    return NULL;
  ptList = pt;
  ::ScreenToClient(w, &ptList);
  return w;
}

static int HitTestItem(HWND list, POINT pt)
{
  LVHITTESTINFO hti;
  memset(&hti, 0, sizeof(hti));
  hti.pt = pt;
  return (int)::SendMessage(list, LVM_HITTEST, 0, (LPARAM)&hti);
}

void CToolbarCustomizeDialog::SetDropHilite(int index)
{
  if (index == _dropHilite)
    return;
  if (_dropHilite >= 0)
    _current.SetItemState(_dropHilite, 0, LVIS_DROPHILITED);
  _dropHilite = index;
  if (_dropHilite >= 0)
    _current.SetItemState(_dropHilite, LVIS_DROPHILITED, LVIS_DROPHILITED);
}

void CToolbarCustomizeDialog::OnDragMove(LPARAM lParam)
{
  POINT pt;
  const HWND w = GetListUnderPoint(*this, lParam, _avail, _current, pt);
  int hilite = -1;
  bool ok = false;
  if (w == (HWND)_current)
  {
    ok = true;
    hilite = HitTestItem(_current, pt);
  }
  else if (w == (HWND)_avail)
    ok = _dragFromCurrent; // drop to "Available": remove
  SetDropHilite(hilite);
  ::SetCursor(ok ? GetDragCursor() : ::LoadCursor(NULL, IDC_NO));
}

void CToolbarCustomizeDialog::EndDrag(bool drop, LPARAM lParam)
{
  if (!_dragging)
    return;
  _dragging = false;
  SetDropHilite(-1);
  if (::GetCapture() == (HWND)*this)
    ::ReleaseCapture();
  if (!drop)
    return;

  POINT pt;
  const HWND w = GetListUnderPoint(*this, lParam, _avail, _current, pt);
  if (!w)
    return;

  if (w == (HWND)_avail)
  {
    // from "Current" to "Available": remove
    if (_dragFromCurrent && _dragIndex >= 0 && (unsigned)_dragIndex < Layout.Size())
    {
      Layout.Delete((unsigned)_dragIndex);
      FillLists(GetSelected(_avail), _dragIndex);
    }
    return;
  }

  // drop to "Current": position before the item under mouse (or at the end)
  int pos = HitTestItem(_current, pt);
  if (pos < 0 || pos > (int)Layout.Size())
    pos = (int)Layout.Size();

  if (_dragFromCurrent)
  {
    // move inside "Current"
    if (_dragIndex < 0 || (unsigned)_dragIndex >= Layout.Size() || pos == _dragIndex)
      return;
    const int k = Layout[(unsigned)_dragIndex];
    Layout.Delete((unsigned)_dragIndex);
    if (pos > _dragIndex)
      pos--;
    if (pos > (int)Layout.Size())
      pos = (int)Layout.Size();
    Layout.Insert((unsigned)pos, k);
    FillLists(GetSelected(_avail), pos);
  }
  else
  {
    // from "Available": add
    if (_dragIndex < 0 || (unsigned)_dragIndex >= _availItems.Size())
      return;
    Layout.Insert((unsigned)pos, _availItems[(unsigned)_dragIndex]);
    FillLists(_dragIndex, pos);
  }
}

bool CToolbarCustomizeDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (_dragging)
  {
    switch (message)
    {
      case WM_MOUSEMOVE:
        OnDragMove(lParam);
        return true;
      case WM_LBUTTONUP:
        EndDrag(true, lParam);
        return true;
      case WM_CAPTURECHANGED:
        EndDrag(false, 0);
        return true;
      case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
          EndDrag(false, 0);
          return true;
        }
        break;
      default:
        break;
    }
  }
  return CModalDialog::OnMessage(message, wParam, lParam);
}

void CToolbarCustomizeDialog::OnOK()
{
  CModalDialog::OnOK();
}
