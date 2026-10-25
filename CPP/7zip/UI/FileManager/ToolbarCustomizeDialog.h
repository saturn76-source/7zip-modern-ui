// ToolbarCustomizeDialog.h
// "Customize Toolbar" window: buttons of toolbar and their order

#ifndef ZIP7_INC_TOOLBAR_CUSTOMIZE_DIALOG_H
#define ZIP7_INC_TOOLBAR_CUSTOMIZE_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ListView.h"

#include "ToolbarCustomizeDialogRes.h"

struct CToolbarButtonDesc
{
  UString Text;
  int ImageIndex;
};

class CToolbarCustomizeDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CListView _avail;
  NWindows::NControl::CListView _current;
  CRecordVector<int> _availItems; // button index, or -1 for separator

  virtual bool OnInit() Z7_override;
  virtual void OnOK() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual bool OnNotify(UINT controlID, LPNMHDR header) Z7_override;
  virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam) Z7_override;

  // drag and drop of items between lists and inside "Current" list
  bool _dragging;
  bool _dragFromCurrent;
  int _dragIndex;
  int _dropHilite;
  void SetDropHilite(int index);
  void EndDrag(bool drop, LPARAM lParam);
  void OnDragMove(LPARAM lParam);

  void InitList(NWindows::NControl::CListView &list);
  void PlaceRightButtons();
  void FillLists(int availSel, int currentSel);
  void UpdateButtons();
  void AddSelected();
  void RemoveSelected();
  void Move(bool up);
public:
  CObjectVector<CToolbarButtonDesc> Buttons; // all buttons
  CRecordVector<int> Layout;                 // in / out: button indexes, -1 is separator
  CRecordVector<int> DefaultLayout;
  HIMAGELIST ImageList;                      // small icons
  UString SeparatorText;

  CToolbarCustomizeDialog(): _dragging(false), _dragFromCurrent(false), _dragIndex(-1), _dropHilite(-1), ImageList(NULL) {}
  INT_PTR Create(HWND parentWindow = NULL) { return CModalDialog::Create(IDD_TB_CUSTOMIZE, parentWindow); }
};

#endif
