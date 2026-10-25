// FindDialog.cpp

#include "StdAfx.h"

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
  CheckButton(IDX_FIND_SUBFOLDERS, Subfolders);
  CheckButton(IDX_FIND_ARCHIVES, ArchivesOnly && ArchivesOnlyEnabled);
  EnableItem(IDX_FIND_ARCHIVES, ArchivesOnlyEnabled);
  NormalizePosition();
  return CModalDialog::OnInit();
}

void CFindDialog::OnOK()
{
  _comboBox.GetText(Value);
  Subfolders = IsButtonCheckedBool(IDX_FIND_SUBFOLDERS);
  ArchivesOnly = IsButtonCheckedBool(IDX_FIND_ARCHIVES);
  CModalDialog::OnOK();
}
