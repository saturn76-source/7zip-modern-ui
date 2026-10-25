// FindDialog.cpp

#include "StdAfx.h"

#include "BrowseDialog.h"
#include "FindDialog.h"

#ifdef Z7_LANG
#include "LangUtils.h"
#endif

using namespace NWindows;

bool CFindDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _comboBox.Attach(GetItem(IDC_FIND_NAME));
  if (!Title.IsEmpty())
    SetText(Title);
  if (!Static.IsEmpty())
    SetItemText(IDT_FIND_NAME, Static);
  _comboBox.SetText(Value);
  FOR_VECTOR (i, Strings)
    _comboBox.AddString(Strings[i]);
  SetItemText(IDE_FIND_LOCATION, Location);
  if (!LocationEditable)
  {
    // inside archive: we search in this archive only
    ::SendMessageW(GetItem(IDE_FIND_LOCATION), EM_SETREADONLY, TRUE, 0);
    EnableItem(IDB_FIND_BROWSE, false);
  }
  CheckButton(IDX_FIND_SUBFOLDERS, Subfolders);
  CheckButton(IDX_FIND_ARCHIVES, ArchivesOnly && ArchivesOnlyEnabled);
  EnableItem(IDX_FIND_ARCHIVES, ArchivesOnlyEnabled);
  NormalizePosition();
  return CModalDialog::OnInit();
}

bool CFindProgressDialog::OnInit()
{
  #ifdef Z7_LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  if (!Title.IsEmpty())
    SetText(Title);
  if (!Status.IsEmpty())
    SetItemText(IDT_FIND_PROGRESS_STATUS, Status);
  if (!Location.IsEmpty())
  {
    UString s ("Search in: ");
    s += Location;
    SetItemText(IDT_FIND_PROGRESS_LOCATION, s);
  }

  _list.Attach(GetItem(IDL_FIND_PROGRESS_LIST));
  _list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
  {
    RECT rc;
    _list.GetClientRect(&rc);
    int w = (int)(rc.right - rc.left) - ::GetSystemMetrics(SM_CXVSCROLL);
    if (w < 100)
      w = 100;
    const int w1 = w * 2 / 5;
    _list.InsertColumn(0, L"Name", w1);
    _list.InsertColumn(1, L"Folder", w - w1);
  }

  NormalizePosition();
  return CModelessDialog::OnInit();
}

void CFindProgressDialog::AddFound(const UStringVector &names, const UStringVector &folders)
{
  if (names.IsEmpty())
    return;
  _list.SetRedraw(false);
  int index = _list.GetItemCount();
  FOR_VECTOR (i, names)
  {
    _list.InsertItem((unsigned)index, names[i]);
    if (i < folders.Size())
      _list.SetSubItem((unsigned)index, 1, folders[i]);
    index++;
  }
  _list.SetRedraw(true);
  ::InvalidateRect(_list, NULL, FALSE);
  // the last found file is visible
  _list.EnsureVisible(index - 1, false);
}

void CFindProgressDialog::SetProgress(const UString &folder, const UString &found)
{
  SetItemText(IDT_FIND_PROGRESS_FOLDER, folder);
  SetItemText(IDT_FIND_PROGRESS_FOUND, found);
}

bool CFindDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  if (buttonID == IDB_FIND_BROWSE)
  {
    UString path;
    GetItemText(IDE_FIND_LOCATION, path);
    UString title;
    GetItemText(IDT_FIND_LOCATION, title);
    title.RemoveChar(L'&');
    UString resultPath;
    if (MyBrowseForFolder(*this, title, path, resultPath))
      SetItemText(IDE_FIND_LOCATION, resultPath);
    return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CFindDialog::OnOK()
{
  _comboBox.GetText(Value);
  GetItemText(IDE_FIND_LOCATION, Location);
  Location.Trim();
  Subfolders = IsButtonCheckedBool(IDX_FIND_SUBFOLDERS);
  ArchivesOnly = IsButtonCheckedBool(IDX_FIND_ARCHIVES);
  CModalDialog::OnOK();
}
