// SelectArchiveDialog.h
// "Easy Extract" without selected archive: user selects archive (name or Browse...)

#ifndef ZIP7_INC_SELECT_ARCHIVE_DIALOG_H
#define ZIP7_INC_SELECT_ARCHIVE_DIALOG_H

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Dialog.h"

#include "SelectArchiveDialogRes.h"

class CSelectArchiveDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _name;
  virtual bool OnInit() Z7_override;
  virtual void OnOK() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  void OnBrowse();
public:
  UString FolderPrefix; // [in]  current folder (with slash at the end)
  UStringVector Names;  // [in]  archives in current folder (for drop-down list)
  UString Path;         // [out] full path of selected archive

  INT_PTR Create(HWND parentWindow = NULL) { return CModalDialog::Create(IDD_SELECT_ARC, parentWindow); }
};

#endif
