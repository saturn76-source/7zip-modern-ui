#include "resourceGui.h"

#define IDI_7Z_FILE   2
#define IDI_ZIP_FILE  3
#define IDI_RAR_FILE  4
#define IDI_ISO_FILE  5

#define IDR_MENUBAR1      70
#define IDM_MENU          71
#define IDR_ACCELERATOR1  72

#define IDB_ADD      100
#define IDB_EXTRACT  101
#define IDB_TEST     102
#define IDB_COPY     103
#define IDB_MOVE     104
#define IDB_DELETE   105
#define IDB_INFO     106

#define IDB_ADD2     150
#define IDB_EXTRACT2 151
#define IDB_TEST2    152
#define IDB_COPY2    153
#define IDB_MOVE2    154
#define IDB_DELETE2  155
#define IDB_INFO2    156

// WinRAR-style toolbar image strips (32-bit with alpha), one per icon height
#define IDB_TB24     170
#define IDB_TB32     171
#define IDB_TB40     172
#define IDB_TB48     173
#define IDB_TB64     174
#define IDB_TB72     175
#define IDB_TB96     176
// small images for ">>" (chevron) menu
#define IDB_TB16     177
#define IDB_TB20     178
// classic 7-Zip icons: large 48x36, small 24x24
#define IDB_TBC_L    179
#define IDB_TBC_S    187

// big "up folder" button images (190 is used by IDB_MENU_LOGO in Explorer/resource2.rc)
#define IDB_UP16     300
#define IDB_UP20     301
#define IDB_UP24     302
#define IDB_UP28     303
#define IDB_UP32     304
#define IDB_UP40     305
#define IDB_UP48     306
#define IDB_UP56     307
#define IDB_UP64     308

#define IDB_LOGO24   180
#define IDB_LOGO32   181
#define IDB_LOGO40   182
#define IDB_LOGO48   183
#define IDB_LOGO64   184
#define IDB_LOGO72   185
#define IDB_LOGO96   186

#define IDM_HASH_ALL             101
#define IDM_CRC32                102
#define IDM_CRC64                103
#define IDM_SHA1                 104
#define IDM_SHA256               105
#define IDM_SHA384               106
#define IDM_SHA512               107
#define IDM_SHA3_256             108
#define IDM_XXH64                120
#define IDM_BLAKE2SP             121
#define IDM_MD5                  122

#define IDM_FILE                 500
#define IDM_EDIT                 501
#define IDM_VIEW                 502
#define IDM_FAVORITES            503
#define IDM_TOOLS                504
#define IDM_HELP                 505

#define IDM_OPEN                 540
#define IDM_OPEN_INSIDE          541
#define IDM_OPEN_OUTSIDE         542
#define IDM_FILE_VIEW            543
#define IDM_FILE_EDIT            544
#define IDM_RENAME               545
#define IDM_COPY_TO              546
#define IDM_MOVE_TO              547
#define IDM_DELETE               548
#define IDM_SPLIT                549
#define IDM_COMBINE              550
#define IDM_PROPERTIES           551
#define IDM_COMMENT              552
#define IDM_CRC                  553
#define IDM_DIFF                 554
#define IDM_CREATE_FOLDER        555
#define IDM_CREATE_FILE          556
// #define IDM_EXIT                 557
#define IDM_LINK                 558
#define IDM_ALT_STREAMS          559
#define IDM_FIND_FILES           570
#define IDM_THEME_LIGHT          571
#define IDM_THEME_DARK           572
#define IDM_THEME_AUTO           573
#define IDM_VIEW_THEME           574
#define IDM_VIEW_TOOLBARS_CUSTOMIZE 575
#define IDM_VIEW_TOOLBARS_CLASSIC   576
#define IDM_EASY_EXTRACT            577

#define IDM_VER_EDIT             580
#define IDM_VER_COMMIT           581
#define IDM_VER_REVERT           582
#define IDM_VER_DIFF             583

#define IDM_OPEN_INSIDE_ONE      590
#define IDM_OPEN_INSIDE_PARSER   591

#define IDM_SELECT_ALL           600
#define IDM_DESELECT_ALL         601
#define IDM_INVERT_SELECTION     602
#define IDM_SELECT               603
#define IDM_DESELECT             604
#define IDM_SELECT_BY_TYPE       605
#define IDM_DESELECT_BY_TYPE     606

#define IDM_VIEW_LARGE_ICONS     700
#define IDM_VIEW_SMALL_ICONS     701
#define IDM_VIEW_LIST            702
#define IDM_VIEW_DETAILS         703

#define IDM_VIEW_ARANGE_BY_NAME  710
#define IDM_VIEW_ARANGE_BY_TYPE  711
#define IDM_VIEW_ARANGE_BY_DATE  712
#define IDM_VIEW_ARANGE_BY_SIZE  713

#define IDM_VIEW_ARANGE_NO_SORT  730
#define IDM_VIEW_FLAT_VIEW       731
#define IDM_VIEW_TWO_PANELS      732
#define IDM_VIEW_TOOLBARS        733
#define IDM_OPEN_ROOT_FOLDER     734
#define IDM_OPEN_PARENT_FOLDER   735
#define IDM_FOLDERS_HISTORY      736
#define IDM_VIEW_REFRESH         737
#define IDM_VIEW_AUTO_REFRESH    738
// #define IDM_VIEW_SHOW_DELETED    739
// #define IDM_VIEW_SHOW_STREAMS    740

#define IDM_VIEW_ARCHIVE_TOOLBAR            750
#define IDM_VIEW_STANDARD_TOOLBAR           751
#define IDM_VIEW_TOOLBARS_LARGE_BUTTONS     752
#define IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT 753

#define IDM_VIEW_TIME_POPUP      760
#define IDM_VIEW_TIME            761
#define IDM_VIEW_TIME_UTC        799

#define IDM_ADD_TO_FAVORITES     800
#define IDS_BOOKMARK             801

#define IDM_OPTIONS              900
#define IDM_BENCHMARK            901
#define IDM_BENCHMARK2           902
#define IDM_TEMP_DIR             910

#define IDM_HELP_CONTENTS        960
#define IDM_ABOUT                961

#define IDS_OPTIONS                     2100

#define IDS_N_SELECTED_ITEMS            3002

#define IDS_FILE_EXIST                  3008
#define IDS_WANT_UPDATE_MODIFIED_FILE   3009
#define IDS_CANNOT_UPDATE_FILE          3010
#define IDS_CANNOT_START_EDITOR         3011
#define IDS_VIRUS                       3012
#define IDS_MESSAGE_UNSUPPORTED_OPERATION_FOR_LONG_PATH_FOLDER  3013
#define IDS_SELECT_ONE_FILE             3014
#define IDS_SELECT_FILES                3015
#define IDS_TOO_MANY_ITEMS              3016

#define IDS_COPY                        6000
#define IDS_MOVE                        6001
#define IDS_COPY_TO                     6002
#define IDS_MOVE_TO                     6003
#define IDS_COPYING                     6004
// #define IDS_MOVING                      6005
#define IDS_RENAMING                    6006

#define IDS_OPERATION_IS_NOT_SUPPORTED  6008
#define IDS_ERROR_RENAMING              6009
#define IDS_CONFIRM_FILE_COPY           6010
#define IDS_WANT_TO_COPY_FILES          6011

#define IDS_CONFIRM_FILE_DELETE         6100
#define IDS_CONFIRM_FOLDER_DELETE       6101
#define IDS_CONFIRM_ITEMS_DELETE        6102
#define IDS_WANT_TO_DELETE_FILE         6103
#define IDS_WANT_TO_DELETE_FOLDER       6104
#define IDS_WANT_TO_DELETE_ITEMS        6105
#define IDS_DELETING                    6106
#define IDS_ERROR_DELETING              6107
#define IDS_ERROR_LONG_PATH_TO_RECYCLE  6108

#define IDS_CREATE_FOLDER               6300
#define IDS_CREATE_FILE                 6301
#define IDS_CREATE_FOLDER_NAME          6302
#define IDS_CREATE_FILE_NAME            6303
#define IDS_CREATE_FOLDER_DEFAULT_NAME  6304
#define IDS_CREATE_FILE_DEFAULT_NAME    6305
#define IDS_CREATE_FOLDER_ERROR         6306
#define IDS_CREATE_FILE_ERROR           6307

#define IDS_COMMENT                     6400
#define IDS_COMMENT2                    6401
#define IDS_SELECT                      6402
#define IDS_DESELECT                    6403
#define IDS_SELECT_MASK                 6404

#define IDS_PROPERTIES                  6600
#define IDS_FOLDERS_HISTORY             6601

#define IDS_COMPUTER                    7100
#define IDS_NETWORK                     7101
#define IDS_DOCUMENTS                   7102
#define IDS_SYSTEM                      7103

#define IDS_ADD                         7200
#define IDS_EXTRACT                     7201
#define IDS_TEST                        7202
#define IDS_BUTTON_COPY                 7203
#define IDS_BUTTON_MOVE                 7204
#define IDS_BUTTON_DELETE               7205
#define IDS_BUTTON_INFO                 7206
#define IDS_BUTTON_VIEW                 7207
#define IDS_BUTTON_FIND                 7208
#define IDS_BUTTON_WIZARD               7209
#define IDS_BUTTON_COMMENT              7210
#define IDS_BUTTON_ABOUT                7211
#define IDS_FIND_TITLE                  7212
#define IDS_FIND_PROMPT                 7213
#define IDS_FIND_RESULT                 7214
#define IDS_FIND_SEARCHING              7215
#define IDS_FIND_NOT_FOUND              7216
#define IDS_FIND_UNSUPPORTED            7217
#define IDS_FIND_TOO_MANY               7218
#define IDS_FIND_FOUND                  7219
#define IDS_FIND_STOPPING               7220
#define IDS_FIND_BAD_LOCATION           7221
#define IDS_EASY_EXTRACT                7222
#define IDS_EASY_EXTRACT_TO             7223
#define IDS_EASY_EXTRACT_OPEN           7224
#define IDS_EASY_EXTRACT_STAR           7225

#define IDI_EASY_EXTRACT  210

#define IDS_SPLITTING                   7303
#define IDS_SPLIT_CONFIRM_TITLE         7304
#define IDS_SPLIT_CONFIRM_MESSAGE       7305
#define IDS_SPLIT_VOL_MUST_BE_SMALLER   7306

#define IDS_COMBINE                     7400
#define IDS_COMBINE_TO                  7401
#define IDS_COMBINING                   7402
#define IDS_COMBINE_SELECT_ONE_FILE     7403
#define IDS_COMBINE_CANT_DETECT_SPLIT_FILE 7404
#define IDS_COMBINE_CANT_FIND_MORE_THAN_ONE_PART 7405
