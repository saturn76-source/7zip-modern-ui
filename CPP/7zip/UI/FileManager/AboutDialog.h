// AboutDialog.h
 
#ifndef ZIP7_INC_ABOUT_DIALOG_H
#define ZIP7_INC_ABOUT_DIALOG_H

#include "../../../Windows/Control/Dialog.h"

#include "AboutDialogRes.h"

class CAboutDialog: public NWindows::NControl::CModalDialog
{
public:
  virtual bool OnInit() Z7_override;
  virtual void OnHelp() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam) Z7_override;
  virtual bool OnTimer(WPARAM timerID, LPARAM callback) Z7_override;
  virtual bool OnDestroy() Z7_override;

  // animated logo
  HICON _icon;
  HDC _iconDC;
  HBITMAP _iconBmp;
  HGDIOBJ _iconOldBmp;
  UInt32 *_iconBits;
  int _iconSize;
  DWORD _startTime;
  DWORD _spinTime;

  bool InitLogo();
  void FreeLogo();
  void DrawLogo(HDC dc, const RECT &rect);
public:
  CAboutDialog():
      _icon(NULL), _iconDC(NULL), _iconBmp(NULL), _iconOldBmp(NULL), _iconBits(NULL),
      _iconSize(0), _startTime(0), _spinTime(0) {}
  INT_PTR Create(HWND wndParent = NULL) { return CModalDialog::Create(IDD_ABOUT, wndParent); }
};

#endif
