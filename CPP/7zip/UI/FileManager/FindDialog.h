// FindDialog.h

#ifndef ZIP7_INC_FIND_DIALOG_H
#define ZIP7_INC_FIND_DIALOG_H

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ListView.h"

#include "FindDialogRes.h"

class CFindDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _comboBox;
  virtual void OnOK() Z7_override;
  virtual bool OnInit() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
public:
  UString Title;
  UString Location;        // folder where we search
  bool LocationEditable;   // false, if we are inside archive
  UString Static;
  UString Value;
  UStringVector Strings;   // history
  bool Subfolders;
  bool ArchivesOnly;
  bool ArchivesOnlyEnabled; // false, if we are inside archive already

  CFindDialog(): LocationEditable(true), Subfolders(true), ArchivesOnly(false), ArchivesOnlyEnabled(true) {}
  INT_PTR Create(HWND parentWindow = NULL) { return CModalDialog::Create(IDD_FIND, parentWindow); }
};

// small window during search: current folder, number of found files, Cancel button
class CFindProgressDialog: public NWindows::NControl::CModelessDialog
{
  NWindows::NControl::CListView _list;
  virtual bool OnInit() Z7_override;
  virtual void OnOK() Z7_override {}
  virtual void OnCancel() Z7_override { CancelPressed = true; }
  virtual void OnClose() Z7_override { CancelPressed = true; }
public:
  bool CancelPressed;
  UString Title;
  UString Status;
  UString Location;

  CFindProgressDialog(): CancelPressed(false) {}
  bool CreateWindow2(HWND parentWindow) { return CModelessDialog::Create(IDD_FIND_PROGRESS, parentWindow); }
  void SetProgress(const UString &folder, const UString &found);
  // adds found files to the list (Name, Folder)
  void AddFound(const UStringVector &names, const UStringVector &folders);
};

#endif
