// FindDialog.h

#ifndef ZIP7_INC_FIND_DIALOG_H
#define ZIP7_INC_FIND_DIALOG_H

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Dialog.h"

#include "FindDialogRes.h"

class CFindDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _comboBox;
  virtual void OnOK() Z7_override;
  virtual bool OnInit() Z7_override;
public:
  UString Title;
  UString Static;
  UString Value;
  UStringVector Strings;   // history
  bool Subfolders;
  bool ArchivesOnly;
  bool ArchivesOnlyEnabled; // false, if we are inside archive already

  CFindDialog(): Subfolders(true), ArchivesOnly(false), ArchivesOnlyEnabled(true) {}
  INT_PTR Create(HWND parentWindow = NULL) { return CModalDialog::Create(IDD_FIND, parentWindow); }
};

#endif
