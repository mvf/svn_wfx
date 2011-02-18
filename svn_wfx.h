#ifndef SVN_WFX_H_INCLUDED
#define SVN_WFX_H_INCLUDED

/*
** svn_wfx - Subversion File System Plugin for Total Commander
** Copyright (C) 2010 Matthias von Faber
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef enum
{
    MSGTYPE_CONNECT           = 1,
    MSGTYPE_DISCONNECT        = 2,
    MSGTYPE_DETAILS           = 3,
    MSGTYPE_TRANSFERCOMPLETE  = 4,
    MSGTYPE_CONNECTCOMPLETE   = 5,
    MSGTYPE_IMPORTANTERROR    = 6,
    MSGTYPE_OPERATIONCOMPLETE = 7
} LogMsgType;

/* flags for tRequestProc */
typedef enum
{
   RQTYPE_OTHER            =  0,
   RQTYPE_USERNAME         =  1,
   RQTYPE_PASSWORD         =  2,
   RQTYPE_ACCOUNT          =  3,
   RQTYPE_USERNAMEFIREWALL =  4,
   RQTYPE_PASSWORDFIREWALL =  5,
   RQTYPE_TARGETDIR        =  6,
   RQTYPE_URL              =  7,
   RQTYPE_MSGOK            =  8,
   RQTYPE_MSGYESNO         =  9,
   RQTYPE_MSGOKCANCEL      = 10
} RequestRqType;


/* ids for FsGetFile */
typedef enum
{
    FS_FILE_OK                        =  0,
    FS_FILE_EXISTS                    =  1,
    FS_FILE_NOTFOUND                  =  2,
    FS_FILE_READERROR                 =  3,
    FS_FILE_WRITEERROR                =  4,
    FS_FILE_USERABORT                 =  5,
    FS_FILE_NOTSUPPORTED              =  6,
    FS_FILE_EXISTSRESUMEALLOWED       =  7,

    FS_COPYFLAGS_OVERWRITE            =  1,
    FS_COPYFLAGS_RESUME               =  2,
    FS_COPYFLAGS_MOVE                 =  4,
    FS_COPYFLAGS_EXISTS_SAMECASE      =  8,
    FS_COPYFLAGS_EXISTS_DIFFERENTCASE = 16
} FsGetFileArg;

/* for FsContentGetSupportedFieldFlags */
typedef enum
{
   CONTFLAGS_EDIT              =  1,
   CONTFLAGS_SUBSTSIZE         =  2,
   CONTFLAGS_SUBSTDATETIME     =  4,
   CONTFLAGS_SUBSTDATE         =  6,
   CONTFLAGS_SUBSTTIME         =  8,
   CONTFLAGS_SUBSTATTRIBUTES   = 10,

   CONTFLAGS_SUBSTATTRIBUTESTR = 12,
   CONTFLAGS_SUBSTMASK         = 14,
} ContFlags;

typedef enum
{
   FT_NOMOREFIELDS     =  0,

   FT_NUMERIC_32       =  1,
   FT_NUMERIC_64       =  2,
   FT_NUMERIC_FLOATING =  3,
   FT_DATE             =  4,
   FT_TIME             =  5,
   FT_BOOLEAN          =  6,
   FT_MULTIPLECHOICE   =  7,
   FT_STRING           =  8,
   FT_FULLTEXT         =  9,
   FT_DATETIME         = 10,
   FT_STRINGW          = 11, /* Should only be returned by Unicode function */

   /* for FsContentGetValue */
   FT_NOSUCHFIELD      = -1, /* error, invalid field number given */
   FT_FILEERROR        = -2, /* file i/o error */
   FT_FIELDEMPTY       = -3  /* field valid, but empty */
} FieldType;

typedef enum
{
    SO_DESCENDING = -1,
    SO_ASCENDING  =  1
} SortOrder;

typedef enum
{
    FS_EXEC_OK       =  0,
    FS_EXEC_ERROR    =  1,
    FS_EXEC_YOURSELF = -1,
    FS_EXEC_SYMLINK  = -2
} ExecResult;

typedef enum
{
    FS_ICON_USEDEFAULT        = 0,
    FS_ICON_EXTRACTED         = 1,
    FS_ICON_EXTRACTED_DESTROY = 2,
    FS_ICON_DELAYED           = 3
} IconResult;

typedef struct
{
    DWORD SizeLow, SizeHigh;
    FILETIME LastWriteTime;
    int Attr;
} RemoteInfoStruct;

typedef struct {

    int size;
    DWORD PluginInterfaceVersionLow;
    DWORD PluginInterfaceVersionHi;
    char DefaultIniName[MAX_PATH];
} FsDefaultParamStruct;

/*
** Callback Types
*/
typedef int  (__stdcall *f_progress_t)(int pluginId, const char *sourceName,    const char *targetName,  int percentDone);
typedef void (__stdcall *f_log_t)     (int pluginId, LogMsgType msgType,        const char *logString);
typedef BOOL (__stdcall *f_request_t) (int pluginId, RequestRqType requestType, const char *customTitle, const char *customText, char *returnedText, int maxLen);


/*
** Exports. Also see svn_wfx.def file.
*/
void       __stdcall FsGetDefRootName(char *dst, int sizeOfDst);

int        __stdcall FsInit(int pluginId, f_progress_t fProgress, f_log_t fLog, f_request_t fRequest);

HANDLE     __stdcall FsFindFirst(char *path, WIN32_FIND_DATA *findData);

BOOL       __stdcall FsFindNext(HANDLE handle, WIN32_FIND_DATA *findData);

int        __stdcall FsFindClose(HANDLE handle);

int        __stdcall FsGetFile(char *remoteName, char *localName, int copyFlags, RemoteInfoStruct *ri);

BOOL       __stdcall FsContentGetDefaultView(char *viewContents, char *viewHeaders, char *viewWidths,char *viewOptions, int maxLen);

SortOrder  __stdcall FsContentGetDefaultSortOrder(int fieldIndex);

FieldType  __stdcall FsContentGetSupportedField(int fieldIndex, char *fieldName, char *units, int maxLen);

int        __stdcall FsContentGetSupportedFieldFlags(int fieldIndex);

int        __stdcall FsContentGetValue(char *fileName, int fieldIndex, int unitIndex, void *fieldValue, int maxLen, int flags);

ExecResult __stdcall FsExecuteFile(HWND mainWin, char *remoteName, char *verb);

int        __stdcall FsExtractCustomIcon(char *remoteName, int extractFlags, HICON *icon);

void       __stdcall FsContentPluginUnloading(void);

void       __stdcall FsSetDefaultParams(FsDefaultParamStruct *dps);

#endif /* !SVN_WFX_H_INCLUDED */
