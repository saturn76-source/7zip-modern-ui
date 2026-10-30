// SelectArchiveDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"

#include "../Common/LoadCodecs.h"

#include "BrowseDialog.h"
#include "LangUtils.h"
#include "SelectArchiveDialog.h"

#include "resource.h"

using namespace NWindows;

extern CCodecs *g_CodecsObj;
HRESULT LoadGlobalCodecs();

bool CSelectArchiveDialog::OnInit()
{
  _name.Attach(GetItem(IDC_SELARC_NAME));
  {
    UString s;
    LangString(IDS_EASY_EXTRACT_SELECT_ARC, s);
    SetItemText(IDT_SELARC_TEXT, s);
  }
  {
    UString s;
    LangString(IDS_EASY_EXTRACT, s);
    if (!s.IsEmpty())
      SetText(s);
  }
  ::SendDlgItemMessageW(*this, IDC_SELARC_ICON, STM_SETICON,
      (WPARAM)::LoadIcon(NULL, IDI_WARNING), 0);
  FOR_VECTOR (i, Names)
    _name.AddString(Names[i]);
  NormalizePosition();
  return CModalDialog::OnInit();
}

bool CSelectArchiveDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  if (buttonID == IDB_SELARC_BROWSE)
  {
    OnBrowse();
    return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CSelectArchiveDialog::OnBrowse()
{
  CObjectVector<CBrowseFilterInfo> filters;
  if (!g_CodecsObj)
    LoadGlobalCodecs();
  if (g_CodecsObj)
  {
    // all archive extensions that 7-Zip can open
    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = "Archives";
    FOR_VECTOR (i, g_CodecsObj->Formats)
    {
      const CArcInfoEx &ai = g_CodecsObj->Formats[i];
      FOR_VECTOR (k, ai.Exts)
      {
        const UString &ext = ai.Exts[k].Ext;
        if (ext.IsEmpty())
          continue;
        UString mask ("*.");
        mask += ext;
        f.Masks.AddToUniqueSorted(mask);
      }
    }
    if (f.Masks.IsEmpty())
      filters.DeleteBack();
  }
  {
    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = "All files";
    f.Masks.Add(UString("*"));
  }

  UString path;
  _name.GetText(path);
  path.Trim();
  if (path.IsEmpty() || !NFile::NName::IsAbsolutePath(path))
    path.Insert(0, FolderPrefix);

  CBrowseInfo bi;
  bi.lpstrTitle = L"Select archive";
  bi.FilterIndex = 0;
  bi.hwndOwner = *this;
  bi.FilePath = path;
  if (!bi.BrowseForFile(filters))
    return;
  _name.SetText(bi.FilePath);
}

void CSelectArchiveDialog::OnOK()
{
  UString path;
  _name.GetText(path);
  path.Trim();
  if (path.IsEmpty())
  {
    _name.SetFocus();
    return;
  }
  if (!NFile::NName::IsAbsolutePath(path))
    path.Insert(0, FolderPrefix);
  NFile::NFind::CFileInfo fi;
  if (!fi.Find(us2fs(path)) || fi.IsDir())
  {
    UString s ("Can not find the file:");
    s.Add_LF();
    s += path;
    ::MessageBoxW(*this, s, L"7-Zip", MB_OK | MB_ICONERROR);
    _name.SetFocus();
    return;
  }
  Path = path;
  CModalDialog::OnOK();
}
