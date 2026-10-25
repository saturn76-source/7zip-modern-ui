// PanelOperations.cpp

#include "StdAfx.h"

#include "../../../Common/DynamicBuffer.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/COM.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "ComboDialog.h"
#include "ListViewDialog.h"
#include "FindDialog.h"

#include "../Common/ArchiveOpenCallback.h"
#include "../Common/LoadCodecs.h"
#include "../Common/OpenArchive.h"

#include <string.h>

extern CCodecs *g_CodecsObj;
HRESULT LoadGlobalCodecs();

#include "FSFolder.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "Panel.h"
#include "UpdateCallback100.h"

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NName;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

enum EFolderOpType
{
  FOLDER_TYPE_CREATE_FOLDER = 0,
  FOLDER_TYPE_DELETE = 1,
  FOLDER_TYPE_RENAME = 2
};

class CThreadFolderOperations: public CProgressThreadVirt
{
  HRESULT ProcessVirt() Z7_override;
public:
  EFolderOpType OpType;
  UString Name;
  UInt32 Index;
  CRecordVector<UInt32> Indices;

  CMyComPtr<IFolderOperations> FolderOperations;
  CMyComPtr<IProgress> UpdateCallback;
  CUpdateCallback100Imp *UpdateCallbackSpec;
  
  CThreadFolderOperations(EFolderOpType opType): OpType(opType) {}
  HRESULT DoOperation(CPanel &panel, const UString &progressTitle, const UString &titleError);
};
  
HRESULT CThreadFolderOperations::ProcessVirt()
{
  NCOM::CComInitializer comInitializer;
  switch ((int)OpType)
  {
    case FOLDER_TYPE_CREATE_FOLDER:
      return FolderOperations->CreateFolder(Name, UpdateCallback);
    case FOLDER_TYPE_DELETE:
      return FolderOperations->Delete(Indices.ConstData(), Indices.Size(), UpdateCallback);
    case FOLDER_TYPE_RENAME:
      return FolderOperations->Rename(Index, Name, UpdateCallback);
    default:
      return E_FAIL;
  }
}


HRESULT CThreadFolderOperations::DoOperation(CPanel &panel, const UString &progressTitle, const UString &titleError)
{
  UpdateCallbackSpec = new CUpdateCallback100Imp;
  UpdateCallback = UpdateCallbackSpec;
  UpdateCallbackSpec->ProgressDialog = this;

  WaitMode = true;
  Sync.FinalMessage.ErrorMessage.Title = titleError;

  UpdateCallbackSpec->Init();

  if (!panel._parentFolders.IsEmpty())
  {
    const CFolderLink &fl = panel._parentFolders.Back();
    UpdateCallbackSpec->PasswordIsDefined = fl.UsePassword;
    UpdateCallbackSpec->Password = fl.Password;
  }

  MainWindow = panel._mainWindow; // panel.GetParent()
  MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
  MainAddTitle = progressTitle + L' ';

  RINOK(Create(progressTitle, MainWindow))
  return Result;
}

#ifndef _UNICODE
typedef int (WINAPI * Func_SHFileOperationW)(LPSHFILEOPSTRUCTW lpFileOp);
#endif

/*
void CPanel::MessageBoxErrorForUpdate(HRESULT errorCode, UINT resourceID)
{
  if (errorCode == E_NOINTERFACE)
    MessageBox_Error_UnsupportOperation();
  else
    MessageBox_Error_HRESULT_Caption(errorCode, LangString(resourceID));
}
*/

void CPanel::DeleteItems(bool NON_CE_VAR(toRecycleBin))
{
  CDisableTimerProcessing disableTimerProcessing(*this);
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.IsEmpty())
    return;
  CSelectedState state;
  SaveSelectedState(state);

  #ifndef UNDER_CE
  // WM6 / SHFileOperationW doesn't ask user! So we use internal delete
  if (IsFSFolder() && toRecycleBin)
  {
    bool useInternalDelete = false;
    #ifndef _UNICODE
    if (!g_IsNT)
    {
      CDynamicBuffer<CHAR> buffer;
      FOR_VECTOR (i, indices)
      {
        const AString path (GetSystemString(GetItemFullPath(indices[i])));
        buffer.AddData(path, path.Len() + 1);
      }
      *buffer.GetCurPtrAndGrow(1) = 0;
      SHFILEOPSTRUCTA fo;
      fo.hwnd = GetParent();
      fo.wFunc = FO_DELETE;
      fo.pFrom = (const CHAR *)buffer;
      fo.pTo = NULL;
      fo.fFlags = 0;
      if (toRecycleBin)
        fo.fFlags |= FOF_ALLOWUNDO;
      // fo.fFlags |= FOF_NOCONFIRMATION;
      // fo.fFlags |= FOF_NOERRORUI;
      // fo.fFlags |= FOF_SILENT;
      // fo.fFlags |= FOF_WANTNUKEWARNING;
      fo.fAnyOperationsAborted = FALSE;
      fo.hNameMappings = NULL;
      fo.lpszProgressTitle = NULL;
      /* int res = */ ::SHFileOperationA(&fo);
    }
    else
    #endif
    {
      CDynamicBuffer<WCHAR> buffer;
      unsigned maxLen = 0;
      const UString prefix = GetFsPath();
      FOR_VECTOR (i, indices)
      {
        // L"\\\\?\\") doesn't work here.
        const UString path = prefix + GetItemRelPath2(indices[i]);
        if (path.Len() > maxLen)
          maxLen = path.Len();
        buffer.AddData(path, path.Len() + 1);
      }
      *buffer.GetCurPtrAndGrow(1) = 0;
      if (maxLen >= MAX_PATH)
      {
        if (toRecycleBin)
        {
          MessageBox_Error_LangID(IDS_ERROR_LONG_PATH_TO_RECYCLE);
          return;
        }
        useInternalDelete = true;
      }
      else
      {
        SHFILEOPSTRUCTW fo;
        fo.hwnd = GetParent();
        fo.wFunc = FO_DELETE;
        fo.pFrom = (const WCHAR *)buffer;
        fo.pTo = NULL;
        fo.fFlags = 0;
        if (toRecycleBin)
          fo.fFlags |= FOF_ALLOWUNDO;
        fo.fAnyOperationsAborted = FALSE;
        fo.hNameMappings = NULL;
        fo.lpszProgressTitle = NULL;
        // int res;
        #ifdef _UNICODE
        /* res = */ ::SHFileOperationW(&fo);
        #else
Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION
        const
        Func_SHFileOperationW
           f_SHFileOperationW = Z7_GET_PROC_ADDRESS(
        Func_SHFileOperationW, ::GetModuleHandleW(L"shell32.dll"),
            "SHFileOperationW");
        if (!f_SHFileOperationW)
          return;
        /* res = */ f_SHFileOperationW(&fo);
        #endif
      }
    }
    /*
    if (fo.fAnyOperationsAborted)
      MessageBox_Error_HRESULT_Caption(result, LangString(IDS_ERROR_DELETING));
    */
    if (!useInternalDelete)
    {
      RefreshListCtrl(state);
      return;
    }
  }
  #endif
 
  // DeleteItemsInternal

  if (!CheckBeforeUpdate(IDS_ERROR_DELETING))
    return;

  UInt32 titleID, messageID;
  UString messageParam;
  if (indices.Size() == 1)
  {
    const unsigned index = indices[0];
    messageParam = GetItemRelPath2(index);
    if (IsItem_Folder(index))
    {
      titleID = IDS_CONFIRM_FOLDER_DELETE;
      messageID = IDS_WANT_TO_DELETE_FOLDER;
    }
    else
    {
      titleID = IDS_CONFIRM_FILE_DELETE;
      messageID = IDS_WANT_TO_DELETE_FILE;
    }
  }
  else
  {
    titleID = IDS_CONFIRM_ITEMS_DELETE;
    messageID = IDS_WANT_TO_DELETE_ITEMS;
    messageParam = NumberToString(indices.Size());
  }
  if (::MessageBoxW(GetParent(), MyFormatNew(messageID, messageParam), LangString(titleID),
      MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
    return;

  CDisableNotify disableNotify(*this);
  {
    CThreadFolderOperations op(FOLDER_TYPE_DELETE);
    op.FolderOperations = _folderOperations;
    op.Indices = indices;
    op.DoOperation(*this,
        LangString(IDS_DELETING),
        LangString(IDS_ERROR_DELETING));
  }
  RefreshTitleAlways();
  RefreshListCtrl(state);
}

BOOL CPanel::OnBeginLabelEdit(LV_DISPINFOW * lpnmh)
{
  const unsigned realIndex = GetRealIndex(lpnmh->item);
  if (realIndex == kParentIndex)
    return TRUE;
  if (IsThereReadOnlyFolder())
    return TRUE;
  return FALSE;
}

static bool IsCorrectFsName(const UString &name)
{
  const UString lastPart = name.Ptr((unsigned)(name.ReverseFind_PathSepar() + 1));
  return
      !lastPart.IsEqualTo(".") &&
      !lastPart.IsEqualTo("..");
}

bool CorrectFsPath(const UString &relBase, const UString &path, UString &result);

bool CPanel::CorrectFsPath(const UString &path2, UString &result)
{
  return ::CorrectFsPath(GetFsPath(), path2, result);
}

BOOL CPanel::OnEndLabelEdit(LV_DISPINFOW * lpnmh)
{
  if (lpnmh->item.pszText == NULL)
    return FALSE;
  CDisableTimerProcessing disableTimerProcessing2(*this);

  if (!CheckBeforeUpdate(IDS_ERROR_RENAMING))
    return FALSE;

  UString newName = lpnmh->item.pszText;
  if (!IsCorrectFsName(newName))
  {
    MessageBox_Error_HRESULT(E_INVALIDARG);
    return FALSE;
  }

  if (IsFSFolder())
  {
    UString correctName;
    if (!CorrectFsPath(newName, correctName))
    {
      MessageBox_Error_HRESULT(E_INVALIDARG);
      return FALSE;
    }
    newName = correctName;
  }

  SaveSelectedState(_selectedState);

  const unsigned realIndex = GetRealIndex(lpnmh->item);
  if (realIndex == kParentIndex)
    return FALSE;
  const UString prefix = GetItemPrefix(realIndex);
  const UString oldName = GetItemName(realIndex);

  CDisableNotify disableNotify(*this);
  {
    CThreadFolderOperations op(FOLDER_TYPE_RENAME);
    op.FolderOperations = _folderOperations;
    op.Index = realIndex;
    op.Name = newName;
    const HRESULT res = op.DoOperation(*this,
        LangString(IDS_RENAMING),
        LangString(IDS_ERROR_RENAMING));
    // fixed in 9.26: we refresh list even after errors
    // (it's more safe, since error can be at different stages, so list can be incorrect).
    if (res == S_OK)
      _selectedState.FocusedName = prefix + newName;
    else
    {
      _selectedState.FocusedName = prefix + oldName;
      // return FALSE;
    }
  }

  // Can't use RefreshListCtrl here.
  // RefreshListCtrlSaveFocused();
  _selectedState.FocusedName_Defined = true;
  _selectedState.SelectFocused = true;

  // We need clear all items to disable GetText before Reload:
  // number of items can change.
  // DeleteListItems();
  // But seems it can still call GetText (maybe for current item)
  // so we can't delete items.

  _dontShowMode = true;

  PostMsg(kReLoadMessage);
  return TRUE;
}

bool Dlg_CreateFolder(HWND wnd, UString &destName);

void CPanel::CreateFolder()
{
  if (IsHashFolder())
    return;

  if (!CheckBeforeUpdate(IDS_CREATE_FOLDER_ERROR))
    return;

  CDisableTimerProcessing disableTimerProcessing2(*this);
  CSelectedState state;
  SaveSelectedState(state);

  UString newName;
  if (!Dlg_CreateFolder(GetParent(), newName))
    return;
  
  if (!IsCorrectFsName(newName))
  {
    MessageBox_Error_HRESULT(E_INVALIDARG);
    return;
  }

  if (IsFSFolder())
  {
    UString correctName;
    if (!CorrectFsPath(newName, correctName))
    {
      MessageBox_Error_HRESULT(E_INVALIDARG);
      return;
    }
    newName = correctName;
  }
  
  HRESULT res;
  CDisableNotify disableNotify(*this);
  {
    CThreadFolderOperations op(FOLDER_TYPE_CREATE_FOLDER);
    op.FolderOperations = _folderOperations;
    op.Name = newName;
    res = op.DoOperation(*this,
        LangString(IDS_CREATE_FOLDER),
        LangString(IDS_CREATE_FOLDER_ERROR));
    /*
    // fixed for 9.26: we must refresh always
    if (res != S_OK)
      return;
    */
  }
  if (res == S_OK)
  {
    int pos = newName.Find(WCHAR_PATH_SEPARATOR);
    if (pos >= 0)
      newName.DeleteFrom((unsigned)(pos));
    if (!_mySelectMode)
      state.SelectedNames.Clear();
    state.FocusedName = newName;
    state.FocusedName_Defined = true;
    state.SelectFocused = true;
  }
  RefreshTitleAlways();
  RefreshListCtrl(state);
}

void CPanel::CreateFile()
{
  if (IsHashFolder())
    return;

  if (!CheckBeforeUpdate(IDS_CREATE_FILE_ERROR))
    return;

  CDisableTimerProcessing disableTimerProcessing2(*this);
  CSelectedState state;
  SaveSelectedState(state);
  CComboDialog dlg;
  LangString(IDS_CREATE_FILE, dlg.Title);
  LangString(IDS_CREATE_FILE_NAME, dlg.Static);
  LangString(IDS_CREATE_FILE_DEFAULT_NAME, dlg.Value);

  if (dlg.Create(GetParent()) != IDOK)
    return;

  CDisableNotify disableNotify(*this);
  
  UString newName = dlg.Value;

  if (IsFSFolder())
  {
    UString correctName;
    if (!CorrectFsPath(newName, correctName))
    {
      MessageBox_Error_HRESULT(E_INVALIDARG);
      return;
    }
    newName = correctName;
  }

  const HRESULT result = _folderOperations->CreateFile(newName, NULL);
  if (result != S_OK)
  {
    MessageBox_Error_HRESULT_Caption(result, LangString(IDS_CREATE_FILE_ERROR));
    // MessageBoxErrorForUpdate(result, IDS_CREATE_FILE_ERROR);
    return;
  }
  const int pos = newName.Find(WCHAR_PATH_SEPARATOR);
  if (pos >= 0)
    newName.DeleteFrom((unsigned)pos);
  if (!_mySelectMode)
    state.SelectedNames.Clear();
  state.FocusedName = newName;
  state.FocusedName_Defined = true;
  state.SelectFocused = true;
  RefreshListCtrl(state);
}

void CPanel::RenameFile()
{
  if (!CheckBeforeUpdate(IDS_ERROR_RENAMING))
    return;
  int index = _listView.GetFocusedItem();
  if (index >= 0)
    _listView.EditLabel(index);
}

void CPanel::ChangeComment()
{
  if (IsHashFolder())
    return;
  if (!CheckBeforeUpdate(IDS_COMMENT))
    return;
  CDisableTimerProcessing disableTimerProcessing2(*this);
  const int index = _listView.GetFocusedItem();
  if (index < 0)
    return;
  const unsigned realIndex = GetRealItemIndex(index);
  if (realIndex == kParentIndex)
    return;
  CSelectedState state;
  SaveSelectedState(state);
  UString comment;
  {
    NCOM::CPropVariant propVariant;
    if (_folder->GetProperty(realIndex, kpidComment, &propVariant) != S_OK)
      return;
    if (propVariant.vt == VT_BSTR)
      comment = propVariant.bstrVal;
    else if (propVariant.vt != VT_EMPTY)
      return;
  }
  const UString name = GetItemRelPath2(realIndex);
  CComboDialog dlg;
  dlg.Title = name;
  dlg.Title += " : ";
  AddLangString(dlg.Title, IDS_COMMENT);
  dlg.Value = comment;
  LangString(IDS_COMMENT2, dlg.Static);
  if (dlg.Create(GetParent()) != IDOK)
    return;
  NCOM::CPropVariant propVariant (dlg.Value);

  CDisableNotify disableNotify(*this);
  const HRESULT result = _folderOperations->SetProperty(realIndex, kpidComment, &propVariant, NULL);
  if (result != S_OK)
  {
    if (result == E_NOINTERFACE)
      MessageBox_Error_UnsupportOperation();
    else
      MessageBox_Error_HRESULT_Caption(result, L"Set Comment Error");
  }
  RefreshListCtrl(state);
}


// ---------- Find files (toolbar "Find" button, Edit / Find..., Ctrl+F) ----------
/*
  It searches the current folder and all its subfolders.
  It works for folders on disk and for folders inside archives
  (it uses IFolderFolder interface, like the panel itself).
  If the name has no wildcards (* ?), it's searched as part of the name: *name*
  Esc key stops the search.
*/

static UString g_FindLastMask;
static bool g_FindSubfolders = true;
static bool g_FindArchivesOnly = false;
static UStringVector g_FindHistory;
static const unsigned kFindHistoryMax = 20;
static const unsigned kFindMaxResults = 20000;
static const unsigned kFindMaxDepth = 64;

struct CFindContext
{
  UString Mask;
  NWindows::NControl::CStatusBar *StatusBar;
  UString SearchingText;
  UStringVector Names;
  UStringVector RelFolders;
  bool Cancelled;
  bool TooMany;
  UInt32 Counter;
  DWORD LastTime;

  CFindContext(): StatusBar(NULL), Cancelled(false), TooMany(false), Counter(0), LastTime(0) {}

  bool NeedStop(const UString &relFolder)
  {
    if (Cancelled || TooMany)
      return true;
    if ((++Counter & 63) != 0)
      return false;
    if (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
      Cancelled = true;
      return true;
    }
    const DWORD t = ::GetTickCount();
    if (t - LastTime >= 200)
    {
      LastTime = t;
      if (StatusBar)
      {
        StatusBar->SetText(0, SearchingText);
        StatusBar->SetText(3, relFolder);
      }
    }
    // keep window painted and "responding" during long search
    MSG msg;
    while (::PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
      ::DispatchMessage(&msg);
    return false;
  }
};

static void FindInFolder(IFolderFolder *folder, const UString &relPrefix,
    bool recursive, unsigned depth, CFindContext &ctx)
{
  UInt32 numItems = 0;
  if (folder->GetNumberOfItems(&numItems) != S_OK)
    return;
  for (UInt32 i = 0; i < numItems; i++)
  {
    if (ctx.NeedStop(relPrefix))
      return;
    UString name;
    {
      NCOM::CPropVariant prop;
      if (folder->GetProperty(i, kpidName, &prop) != S_OK || prop.vt != VT_BSTR)
        continue;
      name = prop.bstrVal;
    }
    bool isDir = false;
    {
      NCOM::CPropVariant prop;
      if (folder->GetProperty(i, kpidIsDir, &prop) == S_OK && prop.vt == VT_BOOL)
        isDir = VARIANT_BOOLToBool(prop.boolVal);
    }
    if (DoesWildcardMatchName(ctx.Mask, name))
    {
      if (ctx.Names.Size() >= kFindMaxResults)
      {
        ctx.TooMany = true;
        return;
      }
      ctx.Names.Add(name);
      ctx.RelFolders.Add(relPrefix);
    }
    if (isDir && recursive && depth < kFindMaxDepth)
    {
      CMyComPtr<IFolderFolder> subFolder;
      if (folder->BindToFolder(i, &subFolder) == S_OK && subFolder)
        if (subFolder->LoadItems() == S_OK)
        {
          UString subPrefix = relPrefix;
          subPrefix += name;
          subPrefix.Add_PathSepar();
          FindInFolder(subFolder, subPrefix, true, depth + 1, ctx);
        }
    }
  }
}

// ---------- search in folders on disk: in separate thread ----------
// UI thread keeps processing messages, so the window doesn't become "Not Responding".

struct CFsFindContext
{
  UString Mask;
  UString RootDir;      // with path separator at the end, can be long path (\\?\)
  bool Recursive;
  bool ArchivesOnly;    // look for items inside archive files
  LONG StopFlag;        // it's changed by both threads: we use Interlocked functions
  bool TooMany;
  CRITICAL_SECTION CS;
  // protected by CS:
  UString CurFolder;
  UStringVector Names;
  UStringVector RelFolders;
  UStringVector ArcRels;   // (ArchivesOnly) relative path of archive for each result
  // only for thread:
  UStringVector ProcessedVolumes; // volumes of multi-volume archives that were opened already

  bool IsStopped() { return ::InterlockedCompareExchange(&StopFlag, 0, 0) != 0; }
  void SetStop() { ::InterlockedExchange(&StopFlag, 1); }

  CFsFindContext(): Recursive(true), ArchivesOnly(false), StopFlag(0), TooMany(false)
    { ::InitializeCriticalSection(&CS); }
  ~CFsFindContext() { ::DeleteCriticalSection(&CS); }
};

// ---------- search inside archives ----------

// archive types that we open in "Search in archives only" mode.
// (disk images like vhd / dmg are not here: they can be very big)
static const char * const k_FindArcExts =
    " 7z zip zipx rar 001 cab iso xz txz lzma tar cpio bz2 bzip2 tbz2 tbz gz gzip tgz tpz"
    " z taz lzh lha rpm deb arj wim swm esd xar zst tzst jar apk ";

static bool IsArchiveFileName(const UString &name)
{
  const int dot = name.ReverseFind_Dot();
  if (dot < 0)
    return false;
  const wchar_t *ext = name.Ptr((unsigned)dot + 1);
  char key[16];
  unsigned k = 0;
  key[k++] = ' ';
  for (;;)
  {
    wchar_t c = *ext++;
    if (c == 0)
      break;
    if (c >= 'A' && c <= 'Z')
      c = (wchar_t)(c + 0x20);
    if (c > 0x7F || k >= sizeof(key) - 2)
      return false;
    key[k++] = (char)c;
  }
  if (k == 1)
    return false;
  key[k++] = ' ';
  key[k] = 0;
  return strstr(k_FindArcExts, key) != NULL;
}

class CFindOpenCallback Z7_final: public IOpenCallbackUI
{
  Z7_IFACE_IMP(IOpenCallbackUI)
public:
  CFsFindContext *Ctx;
  CFindOpenCallback(): Ctx(NULL) {}
};

HRESULT CFindOpenCallback::Open_CheckBreak()
{
  return Ctx->IsStopped() ? E_ABORT : S_OK;
}

HRESULT CFindOpenCallback::Open_SetTotal(const UInt64 * /* files */, const UInt64 * /* bytes */)
{
  return Open_CheckBreak();
}

HRESULT CFindOpenCallback::Open_SetCompleted(const UInt64 * /* files */, const UInt64 * /* bytes */)
{
  return Open_CheckBreak();
}

HRESULT CFindOpenCallback::Open_Finished()
{
  return S_OK;
}

#ifndef Z7_NO_CRYPTO
HRESULT CFindOpenCallback::Open_CryptoGetTextPassword(BSTR *password)
{
  // archive with encrypted file names: we can't look inside it without password. We skip it.
  *password = NULL;
  return E_ABORT;
}
#endif

static void FindInArchive(CFsFindContext &ctx, const UString &rel, const UString &arcName)
{
  if (!g_CodecsObj || ctx.IsStopped())
    return;
  UString arcPath = ctx.RootDir;
  arcPath += rel;
  arcPath += arcName;
  // next volume of multi-volume archive, that was opened already
  if (ctx.ProcessedVolumes.FindInSorted(arcPath) >= 0)
    return;

  const UString arcRel = rel + arcName;
  ::EnterCriticalSection(&ctx.CS);
  ctx.CurFolder = arcRel;
  ::LeaveCriticalSection(&ctx.CS);

  CObjectVector<COpenType> types;
  CIntVector excludedFormats;
  #ifndef Z7_SFX
  CObjectVector<CProperty> props;
  #endif
  COpenOptions op;
  #ifndef Z7_SFX
  op.props = &props;
  #endif
  op.codecs = g_CodecsObj;
  op.types = &types;
  op.excludedFormats = &excludedFormats;
  op.stdInMode = false;
  op.stream = NULL;
  op.filePath = arcPath;

  CFindOpenCallback callback;
  callback.Ctx = &ctx;
  CArchiveLink arcLink;
  HRESULT res = S_OK;
  try { res = arcLink.Open_Strict(op, &callback); }
  catch(...) { return; }
  if (res != S_OK || arcLink.Arcs.IsEmpty())
    return;

  FOR_VECTOR (v, arcLink.VolumePaths)
    ctx.ProcessedVolumes.AddToUniqueSorted(arcLink.VolumePaths[v]);

  const CArc &arc = arcLink.Arcs.Back();
  IInArchive *archive = arc.Archive;
  if (!archive)
    return;
  UInt32 numItems = 0;
  if (archive->GetNumberOfItems(&numItems) != S_OK)
    return;

  UString path;
  for (UInt32 i = 0; i < numItems; i++)
  {
    if (ctx.IsStopped())
      break;
    if (arc.GetItem_Path2(i, path) != S_OK)
      continue;
    if (arc.Ask_AltStream)
    {
      bool isAltStream = false;
      if (Archive_IsItem_AltStream(archive, i, isAltStream) == S_OK && isAltStream)
        continue;
    }
    const int sep = path.ReverseFind_PathSepar();
    const UString name (path.Ptr((unsigned)(sep + 1)));
    if (name.IsEmpty() || !DoesWildcardMatchName(ctx.Mask, name))
      continue;
    UString dir (path);
    dir.DeleteFrom((unsigned)(sep + 1));
    UString folder = arcRel;
    folder.Add_PathSepar();
    folder += dir;
    ::EnterCriticalSection(&ctx.CS);
    if (ctx.Names.Size() >= kFindMaxResults)
    {
      ctx.TooMany = true;
      ctx.SetStop();
    }
    else
    {
      ctx.Names.Add(name);
      ctx.RelFolders.Add(folder);
      ctx.ArcRels.Add(arcRel);
    }
    ::LeaveCriticalSection(&ctx.CS);
  }
}

static void FsFind(CFsFindContext &ctx, const UString &rel, unsigned depth)
{
  if (ctx.IsStopped())
    return;
  ::EnterCriticalSection(&ctx.CS);
  ctx.CurFolder = rel;
  ::LeaveCriticalSection(&ctx.CS);

  UString pattern = ctx.RootDir;
  pattern += rel;
  pattern += L'*';

  WIN32_FIND_DATAW fd;
  // 1 : FindExInfoBasic (no short names), 2 : FIND_FIRST_EX_LARGE_FETCH (Windows 7+)
  HANDLE h = ::FindFirstFileExW(pattern, (FINDEX_INFO_LEVELS)1, &fd,
      FindExSearchNameMatch, NULL, 2);
  if (h == INVALID_HANDLE_VALUE)
    h = ::FindFirstFileW(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return;

  UStringVector subDirs;
  UStringVector arcFiles;
  do
  {
    if (ctx.IsStopped())
      break;
    const wchar_t *n = fd.cFileName;
    if (n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0)))
      continue;
    const UString name (n);
    const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (ctx.ArchivesOnly)
    {
      if (!isDir && IsArchiveFileName(name))
        arcFiles.Add(name);
    }
    else if (DoesWildcardMatchName(ctx.Mask, name))
    {
      ::EnterCriticalSection(&ctx.CS);
      if (ctx.Names.Size() >= kFindMaxResults)
      {
        ctx.TooMany = true;
        ctx.SetStop();
      }
      else
      {
        ctx.Names.Add(name);
        ctx.RelFolders.Add(rel);
      }
      ::LeaveCriticalSection(&ctx.CS);
    }
    // we don't go into junctions and symbolic links: they can make endless loops
    if (isDir && ctx.Recursive
        && (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
      subDirs.Add(name);
  }
  while (::FindNextFileW(h, &fd));
  ::FindClose(h);

  FOR_VECTOR (a, arcFiles)
  {
    if (ctx.IsStopped())
      return;
    FindInArchive(ctx, rel, arcFiles[a]);
  }

  if (depth >= kFindMaxDepth)
    return;
  FOR_VECTOR (i, subDirs)
  {
    if (ctx.IsStopped())
      return;
    UString subRel = rel;
    subRel += subDirs[i];
    subRel.Add_PathSepar();
    FsFind(ctx, subRel, depth + 1);
  }
}

static DWORD WINAPI FsFindThread(LPVOID param)
{
  CFsFindContext &ctx = *(CFsFindContext *)param;
  try { FsFind(ctx, UString(), 0); }
  catch(...) {}
  return 0;
}

// "C:\dir\" -> "\\?\C:\dir\" , "\\server\share\" -> "\\?\UNC\server\share\"
static UString GetLongPathForFind(const UString &path)
{
  if (path.IsPrefixedBy(L"\\\\?\\") || path.IsPrefixedBy(L"\\\\.\\"))
    return path;
  if (path.Len() >= 2 && path.Ptr()[1] == L':')
  {
    UString s (L"\\\\?\\");
    s += path;
    return s;
  }
  if (path.IsPrefixedBy(L"\\\\"))
  {
    UString s (L"\\\\?\\UNC\\");
    s += path.Ptr(2);
    return s;
  }
  return path;
}

// searches in thread; UI messages are processed while waiting
static void FindFiles_FS(HWND mainWindow, NWindows::NControl::CStatusBar &statusBar,
    const UString &searchingText, CFsFindContext &ctx)
{
  DWORD threadID = 0;
  HANDLE thread = ::CreateThread(NULL, 0, FsFindThread, &ctx, 0, &threadID);
  if (!thread)
  {
    FsFind(ctx, UString(), 0);
    return;
  }
  ::EnableWindow(mainWindow, FALSE);
  DWORD lastTime = 0;
  for (;;)
  {
    const DWORD res = ::MsgWaitForMultipleObjects(1, &thread, FALSE, 100, QS_ALLINPUT);
    if (res == WAIT_OBJECT_0)
      break;
    MSG msg;
    while (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
      {
        ctx.SetStop();
        ::PostQuitMessage((int)msg.wParam);
        break;
      }
      if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        ctx.SetStop();
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
    if (::GetForegroundWindow() == mainWindow && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000))
      ctx.SetStop();
    const DWORD t = ::GetTickCount();
    if (t - lastTime >= 200)
    {
      lastTime = t;
      UString folder;
      unsigned num;
      ::EnterCriticalSection(&ctx.CS);
      folder = ctx.CurFolder;
      num = ctx.Names.Size();
      ::LeaveCriticalSection(&ctx.CS);
      UString s = searchingText;
      s.Add_Space();
      s.Add_UInt32(num);
      statusBar.SetText(0, s);
      statusBar.SetText(3, folder);
    }
  }
  ::WaitForSingleObject(thread, INFINITE);
  ::CloseHandle(thread);
  ::EnableWindow(mainWindow, TRUE);
  ::SetForegroundWindow(mainWindow);
}

void CPanel::FindFiles()
{
  if (!_folder || (!IsFSFolder() && !IsArcFolder()))
  {
    MessageBox_Error(LangString(IDS_FIND_UNSUPPORTED));
    return;
  }

  UString mask;
  {
    CFindDialog dlg;
    LangString(IDS_FIND_TITLE, dlg.Title);
    LangString(IDS_FIND_PROMPT, dlg.Static);
    dlg.Value = g_FindLastMask;
    dlg.Strings = g_FindHistory;
    dlg.Subfolders = g_FindSubfolders;
    dlg.ArchivesOnly = g_FindArchivesOnly;
    dlg.ArchivesOnlyEnabled = IsFSFolder(); // inside archive we search in this archive
    if (dlg.Create(GetParent()) != IDOK)
      return;
    mask = dlg.Value;
    g_FindSubfolders = dlg.Subfolders;
    if (dlg.ArchivesOnlyEnabled)
      g_FindArchivesOnly = dlg.ArchivesOnly;
  }
  mask.Trim();
  if (mask.IsEmpty())
    return;

  g_FindLastMask = mask;
  {
    for (unsigned i = 0; i < g_FindHistory.Size();)
      if (g_FindHistory[i].IsEqualTo_NoCase(mask))
        g_FindHistory.Delete(i);
      else
        i++;
    g_FindHistory.Insert(0, mask);
    if (g_FindHistory.Size() > kFindHistoryMax)
      g_FindHistory.DeleteFrom(kFindHistoryMax);
  }

  const UString shownMask = mask;
  if (mask.Find(L'*') < 0 && mask.Find(L'?') < 0)
  {
    mask.InsertAtFront(L'*');
    mask += L'*';
  }

  const UString baseFolder = _currentFolderPrefix;
  UStringVector arcRels; // for results inside archives (search in archives mode)

  CFindContext ctx;
  ctx.Mask = mask;
  ctx.StatusBar = &_statusBar;
  LangString(IDS_FIND_SEARCHING, ctx.SearchingText);
  if (IsFSFolder())
  {
    // folder on disk: fast search in separate thread
    CDisableTimerProcessing disableTimerProcessing(*this);
    CFsFindContext fs;
    fs.Mask = mask;
    fs.RootDir = GetLongPathForFind(baseFolder);
    fs.Recursive = !_flatMode && g_FindSubfolders;
    fs.ArchivesOnly = g_FindArchivesOnly;
    if (fs.ArchivesOnly)
      LoadGlobalCodecs();
    _statusBar.SetText(0, ctx.SearchingText);
    _statusBar.SetText(3, L"");
    FindFiles_FS(GetParent(), _statusBar, ctx.SearchingText, fs);
    _statusBar.SetText(3, L"");
    ctx.Names = fs.Names;
    ctx.RelFolders = fs.RelFolders;
    arcRels = fs.ArcRels;
    ctx.TooMany = fs.TooMany;
    ctx.Cancelled = fs.IsStopped() && !fs.TooMany;
  }
  else
  {
    // archive: items are in memory, so it's fast
    CDisableTimerProcessing disableTimerProcessing(*this);
    HCURSOR oldCursor = ::SetCursor(::LoadCursor(NULL, IDC_WAIT));
    _statusBar.SetText(0, ctx.SearchingText);
    _statusBar.SetText(3, L"");
    FindInFolder(_folder, UString(), !_flatMode && g_FindSubfolders, 0, ctx);
    ::SetCursor(oldCursor);
    _statusBar.SetText(3, L"");
  }
  Refresh_StatusBar();

  if (ctx.Names.IsEmpty())
  {
    if (!ctx.Cancelled)
      ::MessageBoxW(GetParent(), LangString(IDS_FIND_NOT_FOUND), LangString(IDS_FIND_TITLE), MB_OK | MB_ICONINFORMATION);
    return;
  }
  if (ctx.TooMany)
    ::MessageBoxW(GetParent(), LangString(IDS_FIND_TOO_MANY), LangString(IDS_FIND_TITLE), MB_OK | MB_ICONINFORMATION);

  CListViewDialog lv;
  lv.NumColumns = 2;
  lv.SelectFirst = true;
  lv.EnterIsOK = true;
  lv.Title = MyFormatNew(IDS_FIND_RESULT, shownMask);
  lv.Title += " (";
  lv.Title.Add_UInt32(ctx.Names.Size());
  lv.Title += ")";
  lv.Strings = ctx.Names;
  FOR_VECTOR (i, ctx.RelFolders)
    lv.Values.Add(baseFolder + ctx.RelFolders[i]);

  if (lv.Create(GetParent()) != IDOK)
    return;
  const int index = lv.FocusedItemIndex;
  if (index < 0 || (unsigned)index >= ctx.Names.Size())
    return;

  const UString name = ctx.Names[(unsigned)index];
  const UString &rel = ctx.RelFolders[(unsigned)index];
  if (!rel.IsEmpty() || _currentFolderPrefix != baseFolder)
  {
    if (BindToPathAndRefresh(baseFolder + rel) != S_OK)
    {
      // item inside archive: if the folder can't be opened, we open the archive itself
      if ((unsigned)index >= arcRels.Size()
          || BindToPathAndRefresh(baseFolder + arcRels[(unsigned)index]) != S_OK)
        return;
    }
  }
  CSelectedState state;
  state.FocusedName = name;
  state.FocusedName_Defined = true;
  state.SelectFocused = true;
  state.SelectedNames.Add(name);
  RefreshListCtrl(state);
  SetFocusToList();
}
