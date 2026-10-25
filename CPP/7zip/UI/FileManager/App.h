// App.h

#ifndef ZIP7_INC_APP_H
#define ZIP7_INC_APP_H

#include "../../../Windows/Control/CommandBar.h"
#include "../../../Windows/Control/ImageList.h"

#include "AppState.h"
#include "Panel.h"

class CApp;

extern CApp g_App;
extern HWND g_HWND;

const unsigned kNumPanelsMax = 2;

extern bool g_IsSmallScreen;

// "Classic Icons" option of toolbar (original 7-Zip icons). Saved in registry.
bool ReadToolbarClassicIcons();
void SaveToolbarClassicIcons(bool classic);

// must be larger than context menu IDs
const int kMenuCmdID_Toolbar_Start = 1070;
const int kMenuCmdID_Plugin_Start = 1100;

enum
{
  kMenuCmdID_Toolbar_Add = kMenuCmdID_Toolbar_Start,
  kMenuCmdID_Toolbar_Extract,
  kMenuCmdID_Toolbar_Test,
  kMenuCmdID_Toolbar_Chevron,   // ">>" (chevron) button: menu with buttons that don't fit
  kMenuCmdID_Toolbar_Separator, // ID of separator in toolbar (it doesn't send commands)
  kMenuCmdID_Toolbar_Reload,    // posted after "Customize Toolbar": rebuild toolbar
  kMenuCmdID_Toolbar_End
};

class CPanelCallbackImp Z7_final: public CPanelCallback
{
  CApp *_app;
  unsigned _index;
public:
  void Init(CApp *app, unsigned index)
  {
    _app = app;
    _index = index;
  }
  virtual void OnTab() Z7_override;
  virtual void SetFocusToPath(unsigned index) Z7_override;
  virtual void OnCopy(bool move, bool copyToSame) Z7_override;
  virtual void OnSetSameFolder() Z7_override;
  virtual void OnSetSubFolder() Z7_override;
  virtual void PanelWasFocused() Z7_override;
  virtual void DragBegin() Z7_override;
  virtual void DragEnd() Z7_override;
  virtual void RefreshTitle(bool always) Z7_override;
};


class CDropTarget;

class CApp
{
public:
  NWindows::CWindow _window;
  bool ShowSystemMenu;
  bool AutoRefresh_Mode;
  // bool ShowDeletedFiles;
  unsigned NumPanels;
  unsigned LastFocusedPanel;

  bool ShowStandardToolbar;
  bool ShowArchiveToolbar;
  bool ShowButtonsLables;
  bool LargeButtons;

  CAppState AppState;
  CPanelCallbackImp m_PanelCallbackImp[kNumPanelsMax];
  CPanel Panels[kNumPanelsMax];

  NWindows::NControl::CImageList _buttonsImageList;
  NWindows::NControl::CImageList _logoImageList;
  NWindows::NControl::CToolBar _chevronBar;
  HFONT _chevronFont;
  unsigned _toolbarIconSize;
  int _hiddenToolbarCmds[32];
  unsigned _numHiddenToolbarCmds;

  #ifdef UNDER_CE
  NWindows::NControl::CCommandBar _commandBar;
  #endif
  NWindows::NControl::CToolBar _toolBar;

  CDropTarget *_dropTargetSpec;
  CMyComPtr<IDropTarget> _dropTarget;

  UString LangString_N_SELECTED_ITEMS;
  
  void ReloadLangItems();

  CApp():
    _window(NULL),
    AutoRefresh_Mode(true),
    NumPanels(2),
    LastFocusedPanel(0)
  {
    _toolbarIconSize = 48;
    _numHiddenToolbarCmds = 0;
    _chevronFont = NULL;
    _toolbarAdjusting = false;
    ClassicIcons = false;
    SetPanels_AutoRefresh_Mode();
  }

  void CreateDragTarget();
  void SetFocusedPanel(unsigned index);
  void DragBegin(unsigned panelIndex);
  void DragEnd();
  
  void OnCopy(bool move, bool copyToSame, unsigned srcPanelIndex);
  void OnSetSameFolder(unsigned srcPanelIndex);
  void OnSetSubFolder(unsigned srcPanelIndex);

  HRESULT CreateOnePanel(unsigned panelIndex, const UString &mainPath, const UString &arcFormat, bool needOpenArc, COpenResult &openRes);
  HRESULT Create(HWND hwnd, const UString &mainPath, const UString &arcFormat, int xSizes[2], bool needOpenArc, COpenResult &openRes);
  void Read();
  void Save();
  void ReleaseApp();

  // void SetFocus(int panelIndex) { Panels[panelIndex].SetFocusToList(); }
  void SetFocusToLastItem() { Panels[LastFocusedPanel].SetFocusToLastRememberedItem(); }
  unsigned GetFocusedPanelIndex() const { return LastFocusedPanel; }
  bool IsPanelVisible(unsigned index) const { return (NumPanels > 1 || index == LastFocusedPanel); }
  CPanel &GetFocusedPanel() Z7_lifetimebound { return Panels[GetFocusedPanelIndex()]; }

  // File Menu
  void OpenItem() { GetFocusedPanel().OpenSelectedItems(true); }
  void OpenItemInside(const wchar_t *type) { GetFocusedPanel().OpenFocusedItemAsInternal(type); }
  void OpenItemOutside() { GetFocusedPanel().OpenSelectedItems(false); }
  void EditItem(bool useEditor) { GetFocusedPanel().EditItem(useEditor); }
  void Rename() { GetFocusedPanel().RenameFile(); }
  void CopyTo() { OnCopy(false, false, GetFocusedPanelIndex()); }
  void MoveTo() { OnCopy(true, false, GetFocusedPanelIndex()); }
  void Delete(bool toRecycleBin) { GetFocusedPanel().DeleteItems(toRecycleBin); }
  HRESULT CalculateCrc2(const UString &methodName);
  void CalculateCrc(const char *methodName);

  void DiffFiles(const UString &path1, const UString &path2);
  void DiffFiles();
  
  void VerCtrl(unsigned id);

  void Split();
  void Combine();
  void Properties() { GetFocusedPanel().Properties(); }
  void Comment() { GetFocusedPanel().ChangeComment(); }
  
  #ifndef UNDER_CE
  void Link();
  void OpenAltStreams() { GetFocusedPanel().OpenAltStreams(); }
  #endif

  void CreateFolder() { GetFocusedPanel().CreateFolder(); }
  void CreateFile() { GetFocusedPanel().CreateFile(); }

  // Edit
  void EditCut() { GetFocusedPanel().EditCut(); }
  void EditCopy() { GetFocusedPanel().EditCopy(); }
  void EditPaste() { GetFocusedPanel().EditPaste(); }

  void SelectAll(bool selectMode) { GetFocusedPanel().SelectAll(selectMode); }
  void InvertSelection() { GetFocusedPanel().InvertSelection(); }
  void SelectSpec(bool selectMode) { GetFocusedPanel().SelectSpec(selectMode); }
  void SelectByType(bool selectMode) { GetFocusedPanel().SelectByType(selectMode); }

  void Refresh_StatusBar() { GetFocusedPanel().Refresh_StatusBar(); }

  void SetListViewMode(UInt32 index) { GetFocusedPanel().SetListViewMode(index); }
  UInt32 GetListViewMode() { return GetFocusedPanel().GetListViewMode(); }
  PROPID GetSortID() { return GetFocusedPanel().GetSortID(); }

  void SortItemsWithPropID(PROPID propID) { GetFocusedPanel().SortItemsWithPropID(propID); }

  void OpenRootFolder() { GetFocusedPanel().OpenDrivesFolder(); }
  void OpenParentFolder() { GetFocusedPanel().OpenParentFolder(); }
  void FoldersHistory() { GetFocusedPanel().FoldersHistory(); }
  void RefreshView() { GetFocusedPanel().OnReload(); }
  void RefreshAllPanels()
  {
    for (unsigned i = 0; i < NumPanels; i++)
    {
      unsigned index = i;
      if (NumPanels == 1)
        index = LastFocusedPanel;
      Panels[index].OnReload();
    }
  }

  /*
  void SysIconsWereChanged()
  {
    for (unsigned i = 0; i < NumPanels; i++)
    {
      unsigned index = i;
      if (NumPanels == 1)
        index = LastFocusedPanel;
      Panels[index].SysIconsWereChanged();
    }
  }
  */

  void SetListSettings();
  HRESULT SwitchOnOffOnePanel();
  
  CIntVector _timestampLevels;

  bool GetFlatMode() { return Panels[LastFocusedPanel].GetFlatMode(); }

  int GetTimestampLevel() const { return Panels[LastFocusedPanel]._timestampLevel; }
  void SetTimestampLevel(int level)
  {
    for (unsigned i = 0; i < kNumPanelsMax; i++)
    {
      CPanel &panel = Panels[i];
      panel._timestampLevel = level;
    }
    RedrawListItems_InPanels();
  }

  void RedrawListItems_InPanels()
  {
    for (unsigned i = 0; i < kNumPanelsMax; i++)
    {
      CPanel &panel = Panels[i];
      if (panel.PanelCreated)
        panel.RedrawListItems();
    }
  }

  // bool Get_ShowNtfsStrems_Mode() { return Panels[LastFocusedPanel].Get_ShowNtfsStrems_Mode(); }
  
  void ChangeFlatMode() { Panels[LastFocusedPanel].ChangeFlatMode(); }
  // void Change_ShowNtfsStrems_Mode() { Panels[LastFocusedPanel].Change_ShowNtfsStrems_Mode(); }
  // void Change_ShowDeleted() { ShowDeletedFiles = !ShowDeletedFiles; }

  bool Get_AutoRefresh_Mode()
  {
    // return Panels[LastFocusedPanel].Get_ShowNtfsStrems_Mode();
    return AutoRefresh_Mode;
  }
  void Change_AutoRefresh_Mode()
  {
    AutoRefresh_Mode = !AutoRefresh_Mode;
    SetPanels_AutoRefresh_Mode();
  }
  void SetPanels_AutoRefresh_Mode()
  {
    for (unsigned i = 0; i < kNumPanelsMax; i++)
      Panels[i].Set_AutoRefresh_Mode(AutoRefresh_Mode);
  }

  void OpenBookmark(unsigned index) { GetFocusedPanel().OpenBookmark(index); }
  void SetBookmark(unsigned index) { GetFocusedPanel().SetBookmark(index); }

  void ReloadToolbars();
  void FillToolbarButtons(const CRecordVector<int> &layout);
  void SaveCurrentToolbarLayout();
  void GetCurrentToolbarLayout(CRecordVector<int> &layout);
  void ShowToolbarContextMenu();
  void CustomizeToolbar();
  bool OnToolbarAdjustNotify(LPNMHDR pnmh, LRESULT &result);
  bool _toolbarAdjusting;
  void CreateChevron();
  void UpdateToolbarOverflow(int width);
  void ShowToolbarOverflowMenu();
  bool ClassicIcons;
  void SwitchClassicIcons()
  {
    ClassicIcons = !ClassicIcons;
    SaveToolbarClassicIcons(ClassicIcons);
    SaveToolbarChanges();
  }
  void ReadToolbar()
  {
    ClassicIcons = ReadToolbarClassicIcons();
    const UInt32 mask = ReadToolbarsMask();
    if (mask & ((UInt32)1 << 31))
    {
      ShowButtonsLables = true;
      LargeButtons = !g_IsSmallScreen;
      ShowStandardToolbar = ShowArchiveToolbar = true;
    }
    else
    {
      ShowButtonsLables = ((mask & 1) != 0);
      LargeButtons = ((mask & 2) != 0);
      ShowStandardToolbar = ((mask & 4) != 0);
      ShowArchiveToolbar  = ((mask & 8) != 0);
    }
  }
  void SaveToolbar()
  {
    UInt32 mask = 0;
    if (ShowButtonsLables) mask |= 1;
    if (LargeButtons) mask |= 2;
    if (ShowStandardToolbar) mask |= 4;
    if (ShowArchiveToolbar) mask |= 8;
    SaveToolbarsMask(mask);
  }
  
  void SaveToolbarChanges();

  void SwitchStandardToolbar()
  {
    ShowStandardToolbar = !ShowStandardToolbar;
    SaveToolbarChanges();
  }
  void SwitchArchiveToolbar()
  {
    // only one toolbar: "View / Toolbars / Toolbar" shows or hides it
    const bool show = !(ShowArchiveToolbar || ShowStandardToolbar);
    ShowArchiveToolbar = show;
    ShowStandardToolbar = show;
    SaveToolbarChanges();
  }
  void SwitchButtonsLables()
  {
    ShowButtonsLables = !ShowButtonsLables;
    SaveToolbarChanges();
  }
  void SwitchLargeButtons()
  {
    LargeButtons = !LargeButtons;
    SaveToolbarChanges();
  }

  void AddToArchive() { GetFocusedPanel().AddToArchive(); }
  void FindFiles() { GetFocusedPanel().FindFiles(); }
  void ApplyTheme();
  void SetThemeMode(int mode);
  void ExtractArchives() { GetFocusedPanel().ExtractArchives(); }
  void TestArchives() { GetFocusedPanel().TestArchives(); }

  bool OnNotify(int ctrlID, LPNMHDR pnmh, LRESULT &result);

  UString PrevTitle;
  void RefreshTitle(bool always = false);
  void RefreshTitleAlways() { RefreshTitle(true); }
  void RefreshTitlePanel(unsigned panelIndex, bool always = false);

  void MoveSubWindows();
};

#endif
