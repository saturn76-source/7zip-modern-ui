// RootFolder.h

#ifndef ZIP7_INC_ROOT_FOLDER_H
#define ZIP7_INC_ROOT_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyString.h"

#include "IFolder.h"

// 4 standard items + Desktop, Downloads, Pictures, Videos
const unsigned kNumRootFolderItems_Max = 8;

Z7_CLASS_IMP_NOQIB_2(
  CRootFolder
  , IFolderFolder
  , IFolderGetSystemIconIndex
)
  UString _names[kNumRootFolderItems_Max];
  int _iconIndices[kNumRootFolderItems_Max];
  UString _paths[kNumRootFolderItems_Max]; // for Desktop, Downloads, Pictures, Videos
  int _kinds[kNumRootFolderItems_Max]; // ROOT_INDEX_* in RootFolder.cpp
  unsigned _numItems;
  void AddItem(int kind, const UString &name, int iconIndex, const UString &path);
  int FindKind(int kind) const;
public:
  CRootFolder(): _numItems(0) {}
  void Init();
};

#endif
