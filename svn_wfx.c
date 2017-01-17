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

#include "svn_wfx.h"
#include "tproc.h"
#include "strbuf.h"

#include <svn_client.h>
#include <svn_fs.h>
#include <svn_pools.h>

#include <Userenv.h>

#include "resource.h"

/*
** Types
*/
typedef struct String
{
   char *data;
   size_t len;
} String;

typedef struct Field
{
    String name;
    FieldType type;
    int flags;
    SortOrder sortOrder;
} Field;

typedef struct Location
{
    String title, url;
    struct Location *next;
} Location;

typedef struct SVNObject
{
    char *name;
    svn_dirent_t dirent;
    struct SVNObject *next;
} SVNObject;

typedef struct Snapshot
{
    const Location *location;
    String subPath;
    SVNObject *entries;
    SVNObject *current;
} Snapshot;

enum FieldIndices
{
    FI_REVISION,
    FI_AUTHOR,
    FI_MAX
};


/*
** Prototypes
*/

/** @return an escaped representation of @a uri, allocated from @a pool. */
static char *escapeURI(const char *uri, apr_pool_t *pool);

/** Callback for svn_client_list2 */
static svn_error_t* list_func(Snapshot *snapshot,
                              const char *path,
                              const svn_dirent_t  *dirent,
                              const svn_lock_t  *lock,
                              const char *abs_path,
                              apr_pool_t *pool);

/** Queries the server for a directory listing of @a path and stores the
    result in @a snapshot. If this function fails, the contents of @a
    snapshot are undefined.
    @param snapshot The destination snapshot.
    @param path The remote path, in TC format, minus the leading backslash.
    @return An error message on failure, or NULL on success. */
static svn_error_t *querySnapshot(Snapshot *snapshot, const char *path);

/** Initializes Subversion.
    @return 0 on success. */
static int initSvn();

/** (Re-)Loads configuration from disk. */
static void loadConfig();

/** Retrieves information about the SVN object.
    @param val The source value.
    @param findData The destination find data. */
static void getSvnNode(SVNObject *obj, WIN32_FIND_DATA *findData);

/** Replaces all occurrences of oldVal with newVal in the zero-terminated string @a str.
    @param str The zero-terminated string.
    @param oldVal The value to look for.
    @param newVal The value to replace any oldVal with. */
static void replaceAll(char *str, char oldVal, char newVal);

/** Replaces all occurrences of '\\' with '/' in the zero-terminated string @a str.
    @param str The zero-terminated string. */
static void slashify(char *str);

/** Destroys the given snapshot. */
static void destroySnapshot(Snapshot *snapshot);

/** Releases all locations and snapshots */
static void freeLocationsAndSnapshots();

/** @return The SVN URI associated with @a remoteName, allocated from @a pool,
    with @a overAllocate additional bytes allocated past the terminating zero,
    or NULL if @a remoteName did not match any known locations. */
static char *remoteNameToSvnURI(char *remoteName, apr_pool_t *pool, size_t overAllocate);

/** Displays an error message box.
    @param msg The message to display. */
static void displayErrorMessage(const char *msg);

/** Reads a single line of input via TC Plugin Request.
    @param prompt The string to the left of the text input widget.
    @param buffer Buffer that contains the default response and receives input.
    @param max The size of @a buffer.
    @param requestType The TC request type. */
static svn_error_t *promptLine(const char *prompt, char *buffer, size_t max, RequestRqType requestType);

/** @see svn_auth_simple_prompt_func_t */
static svn_error_t *promptCallback(svn_auth_cred_simple_t **cred, void *baton, const char *realm, const char *username, svn_boolean_t may_save, apr_pool_t *pool);

/** @see svn_auth_username_prompt_func_t */
static svn_error_t *promptCallbackUsername(svn_auth_cred_username_t **cred, void *baton, const char *realm, svn_boolean_t may_save, apr_pool_t *pool);

/*
** Globals
*/
static const String ConfigFileName     = { "svn_wfx.ini"   , 11 };
static const String EditLocationsTitle = { "Edit Locations", 14 };

static HINSTANCE hInstance;

static struct
{
    Location *locations;
    String configFilePath;
} Config = { 0 };

static const Field fields[] =
{
    {
        /* name  */     { "revision", 8 },
#if (defined WIN64)
        /* type  */     FT_NUMERIC_64,
#elif (defined WIN32)
        /* type  */     FT_NUMERIC_32,
#else
#error Unsupported platform!
#endif
        /* flags */     0,
        /* sortOrder */ SO_DESCENDING
    },
    {
        /* name  */     { "author", 6 },
        /* type  */     FT_STRING,
        /* flags */     0,
        /* sortOrder */ SO_ASCENDING
    }
};

static struct
{
    int          id;
    f_progress_t progress;
    f_log_t      log;
    f_request_t  request;
} Plugin = { 0 };

static struct
{
    apr_pool_t *pool;
    svn_client_ctx_t *ctx;
    svn_error_t *error;
} Subversion = { 0 };

static Location *nextTopLevelLoc;

static Snapshot cachedSnapshot;

/*
** Implementation
*/

/*--------------------------------------------------------------------------*/
void __stdcall FsGetDefRootName(char *dst, int sizeOfDst)
{
    strbuf_t s = { dst, sizeOfDst };
    strbuf_cat(&s, "Subversion", 10);
}

/*--------------------------------------------------------------------------*/
int __stdcall FsInit(int pluginId, f_progress_t fProgress, f_log_t fLog, f_request_t fRequest)
{
    Plugin.id       = pluginId;
    Plugin.progress = fProgress;
    Plugin.log      = fLog;
    Plugin.request  = fRequest;
    memset(&cachedSnapshot, 0, sizeof(cachedSnapshot));
    tproc_init(&displayErrorMessage);
    return initSvn();
}

/*--------------------------------------------------------------------------*/
HANDLE __stdcall FsFindFirst(char* path, WIN32_FIND_DATA *findData)
{
    const size_t pathLen = strlen(path);

    if (*path++ != '\\' )
    {
        return INVALID_HANDLE_VALUE;
    }
    memset(findData, 0, sizeof(*findData));
    if (*path)
    {
        /* nested directory */
        Snapshot *snapshot = malloc(sizeof(Snapshot));
        svn_error_t *err = querySnapshot(snapshot, path);
        if (err)
        {
            if (err->message)
            {
                char buf[1024];
                strbuf_t s = { buf, sizeof(buf) };

                strbuf_cat(&s, err->message, strlen(err->message));
                if (err->child && err->child->message)
                {
                    strbuf_cat(&s, "\n\n", 2);
                    strbuf_cat(&s, err->child->message, strlen(err->child->message));
                }
                MessageBox(NULL, buf, "SVN Error", MB_OK | MB_ICONERROR);
            }
            svn_error_clear(err);
        }
        else
        {
            if (snapshot->entries)
            {
                getSvnNode(snapshot->entries, findData);
                snapshot->current = snapshot->entries->next;

                return (HANDLE) snapshot;
            }
            SetLastError(ERROR_NO_MORE_FILES);
        }
        free(snapshot);
    }
    else
    {
        /* root directory */
        memcpy(findData->cFileName, EditLocationsTitle.data, EditLocationsTitle.len + 1);
        findData->dwFileAttributes = FILE_ATTRIBUTE_READONLY;
        nextTopLevelLoc = Config.locations;
        return (HANDLE) 0;
    }
    return INVALID_HANDLE_VALUE;
}

/*--------------------------------------------------------------------------*/
BOOL __stdcall FsFindNext(HANDLE handle, WIN32_FIND_DATA *findData)
{
    Snapshot *snapshot = (Snapshot*) handle;
    if (snapshot)
    {
        if (snapshot->current)
        {
            getSvnNode(snapshot->current, findData);
            snapshot->current = snapshot->current->next;
            return TRUE;
        }
        snapshot->current = snapshot->entries;
    }
    else
    {
        if (nextTopLevelLoc)
        {
            memcpy(findData->cFileName, nextTopLevelLoc->title.data, nextTopLevelLoc->title.len + 1);
            findData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY;
            nextTopLevelLoc = nextTopLevelLoc->next;
            return TRUE;
        }
    }
    return FALSE;
}

/*--------------------------------------------------------------------------*/
int __stdcall FsFindClose(HANDLE handle)
{
    if (handle)
    {
        Snapshot *snapshot = (Snapshot*) handle;
        destroySnapshot(&cachedSnapshot);
        cachedSnapshot.location = snapshot->location;
        cachedSnapshot.subPath = snapshot->subPath;
        cachedSnapshot.entries = snapshot->entries;
        free(handle);
    }
    else
    {
        nextTopLevelLoc = Config.locations;
    }
    return 0;
}

/*--------------------------------------------------------------------------*/
int __stdcall FsGetFile(char *remoteName, char *localName, int copyFlags, RemoteInfoStruct *ri)
{
    apr_pool_t *subPool;
    svn_opt_revision_t revision;
    apr_file_t *file;
    svn_stream_t *stream;
    char *uri;

    if (*remoteName++ != '\\' )
    {
        return FS_FILE_NOTFOUND;
    }

    subPool = svn_pool_create(Subversion.pool);
    uri = remoteNameToSvnURI(remoteName, subPool, 0);
    if (!uri)
    {
        return svn_pool_destroy(subPool), FS_FILE_NOTFOUND;
    }

    if (!(copyFlags & FS_COPYFLAGS_OVERWRITE))
    {
        HANDLE hFile = CreateFile(localName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            return svn_pool_destroy(subPool), FS_FILE_EXISTS;
        }
    }

    Plugin.progress(Plugin.id, uri, localName, 0);
    revision.kind = svn_opt_revision_head;

    slashify(localName);
    {
        apr_status_t apr_status = apr_file_open(&file, localName, APR_WRITE | APR_CREATE | APR_TRUNCATE | APR_BINARY, APR_OS_DEFAULT, Subversion.pool);
        replaceAll(localName, '/', '\\');
        if (apr_status)
        {
            char buf[1024];
            apr_strerror(apr_status, buf, sizeof(buf));
            MessageBox(NULL, buf, "apr_file_open", MB_OK | MB_ICONERROR);
            return svn_pool_destroy(subPool), FS_FILE_WRITEERROR;
        }
    }
    stream = svn_stream_from_aprfile2(file, FALSE, Subversion.pool);
    {
        svn_error_t *svn_error = svn_client_cat(stream, escapeURI(uri, subPool), &revision, Subversion.ctx, Subversion.pool);
        if (svn_error)
        {
            MessageBox(NULL, svn_error->message, "svn_client_cat", MB_OK | MB_ICONERROR);
            apr_file_close(file);
            return svn_pool_destroy(subPool), FS_FILE_READERROR;
        }
    }

    svn_stream_close(stream);
    Plugin.progress(Plugin.id, uri, localName, 100);

    return svn_pool_destroy(subPool), FS_FILE_OK;
}

/*--------------------------------------------------------------------------*/
BOOL __stdcall FsContentGetDefaultView(char *viewContents, char *viewHeaders, char *viewWidths,char *viewOptions, int maxLen)
{
    static const char Contents[] = "[=tc.size]\\n[=<fs>.revision]\\n[=<fs>.author]\\n[=tc.writedate]";
    static const char Headers[]  = "Size\\nRevision\\nAuthor\\nDate";
    static const char Widths[]   = "148,23,-40,-40,40,-80";
    static const char Options[]  = "-1|0";

    strbuf_t s;

    strbuf_cat(strbuf_init(&s, viewContents, maxLen), Contents, sizeof(Contents) - 1);
    strbuf_cat(strbuf_init(&s, viewHeaders,  maxLen), Headers,  sizeof(Headers)  - 1);
    strbuf_cat(strbuf_init(&s, viewWidths,   maxLen), Widths,   sizeof(Widths)   - 1);
    strbuf_cat(strbuf_init(&s, viewOptions,  maxLen), Options,  sizeof(Options)  - 1);  /* auto-adjust-width, or -1 for no adjust | horizontal scrollbar flag */

    return TRUE;
}

/*--------------------------------------------------------------------------*/
SortOrder __stdcall FsContentGetDefaultSortOrder(int fieldIndex)
{
    if ((fieldIndex >= 0) && (fieldIndex < FI_MAX))
    {
        return fields[fieldIndex].sortOrder;
    }
    return SO_ASCENDING;
}

/*--------------------------------------------------------------------------*/
FieldType __stdcall FsContentGetSupportedField(int fieldIndex, char *fieldName, char *units, int maxLen)
{
    if ((fieldIndex >= 0) && (fieldIndex < FI_MAX))
    {
        const Field *field = fields + fieldIndex;
        strbuf_t s = { fieldName, maxLen };
        strbuf_cat(&s, field->name.data, field->name.len);
        if (maxLen)
        {
            *units = '\0';
        }
        return field->type;
    }
    return FT_NOMOREFIELDS;
}

/*--------------------------------------------------------------------------*/
int __stdcall FsContentGetSupportedFieldFlags(int fieldIndex)
{
    if ((fieldIndex >= 0) && (fieldIndex < FI_MAX))
    {
        return fields[fieldIndex].flags;
    }
    return CONTFLAGS_SUBSTMASK;
}

/*--------------------------------------------------------------------------*/
int __stdcall FsContentGetValue(char *fileName, int fieldIndex, int unitIndex, void *fieldValue, int maxLen, int flags)
{
    const Field *field = fields + fieldIndex;
    char *baseFileName, *baseFilePath;
    Snapshot *snapshot = &cachedSnapshot;

    if ((fieldIndex < 0) || (fieldIndex >= FI_MAX) || *fileName++ != '\\')
    {
        return FT_NOSUCHFIELD;
    }

    baseFileName = strrchr(fileName, '\\');

    baseFilePath = strchr(fileName, '\\');

    if (!baseFileName++ || !baseFilePath)
    {
        return FT_NOSUCHFIELD;
    }

    slashify(fileName);

    if (   !snapshot->entries
        || !snapshot->subPath.data
        || strncmp(fileName, snapshot->location->title.data, snapshot->location->title.len)
        || strncmp(snapshot->subPath.data, baseFilePath, snapshot->subPath.len))
    {
        const char tmp = *baseFileName;
        svn_error_t *err;
        destroySnapshot(snapshot);
        *baseFileName = '\0';
        err = querySnapshot(snapshot, fileName);
        *baseFileName = tmp;
        if (err)
        {
            if (err->message)
            {
                displayErrorMessage(err->message);
            }
            svn_error_clear(err);
            return FT_FILEERROR;
        }
    }

    for (snapshot->current = snapshot->entries; snapshot->current; snapshot->current = snapshot->current->next)
    {
        if (!strcmp(baseFileName, snapshot->current->name))
            break;
    }

    if (!snapshot->current)
    {
        return FT_NOSUCHFIELD;
    }

    switch (fieldIndex)
    {
        case FI_REVISION:
            *((long*)fieldValue) = snapshot->current->dirent.created_rev;
            break;
        case FI_AUTHOR:
            if (snapshot->current->dirent.last_author)
            {
                strbuf_t s = { (char*) fieldValue, maxLen };
                strbuf_cat(&s, snapshot->current->dirent.last_author, strlen(snapshot->current->dirent.last_author));
            }
            break;
    }

    return field->type;
}

/*--------------------------------------------------------------------------*/
ExecResult __stdcall FsExecuteFile(HWND mainWin, char *remoteName, char *verb)
{
    apr_pool_t *subPool;

    if (!remoteName || *remoteName++ != '\\' )
    {
        return FS_EXEC_ERROR;
    }

    subPool = svn_pool_create(Subversion.pool);

    if (!strnicmp(verb, "open", 4))
    {
        if (!strcmp(remoteName, EditLocationsTitle.data))
        {
            static const char * const EnvEditor = "EDITOR";
            static const String DefaultEditor = { "notepad.exe", 11 };
            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            const DWORD editorPathLen = GetEnvironmentVariable(EnvEditor, NULL, 0);
            /* length of editor command + strlen(" \"") + length of config file path + strlen("\"") + 1 */
            const size_t bufferSize = (editorPathLen ? editorPathLen : DefaultEditor.len) + 2 + Config.configFilePath.len + 2;

            char *buf = apr_palloc(subPool, bufferSize);
            strbuf_t s = { buf, bufferSize };
            GetStartupInfo(&si);
            strbuf_adv(&s, GetEnvironmentVariable(EnvEditor, s.data, s.size));
            if (s.data == buf)
            {
                strbuf_cat(&s, DefaultEditor.data, DefaultEditor.len);
            }
            strbuf_cat(&s, " \"", 2);
            strbuf_cat(&s, Config.configFilePath.data, Config.configFilePath.len);
            strbuf_cat(&s, "\"", 1);

            CreateProcess(NULL, buf, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            CloseHandle(pi.hThread);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            loadConfig();

            return svn_pool_destroy(subPool), FS_EXEC_OK;
        }
        return svn_pool_destroy(subPool), FS_EXEC_YOURSELF;
    }
    else if (*remoteName && !strnicmp(verb, "quote ", 6))
    {
        static const struct Command
        {
            String cmd;
            const char *helpParam, *helpText;
            void (*proc)(const char *);
        } commands[] = {
            { { "co",    2 }, "[srcdir]", "Open Checkout dialog",       &tproc_checkout    },
            { { "blame", 5 }, "<file>",   "Open Blame dialog",          &tproc_blame       },
            { { "log",   3 }, "[path]",   "Open Log dialog",            &tproc_log         },
            { { "props", 5 }, "[path]",   "Open SVN properties dialog", &tproc_props       },
            { { "rb",    2 }, "[path]",   "Open Repository Browser",    &tproc_repobrowser },
            { { "rg",    2 }, "[path]",   "Open Revision Graph",        &tproc_revgraph    },
            { { NULL,    0 }, NULL,       NULL,                         NULL               }
        };
        const struct Command *command = commands;
        char *buf;

        verb += 6; /* skip "quote " */
        while (command->cmd.data)
        {
            if (!strnicmp(verb, command->cmd.data, command->cmd.len))
            {
                size_t argLen;
                verb += command->cmd.len;
                while (isspace(*verb) || *verb == '"') ++verb;
                argLen = strlen(verb);
                buf = remoteNameToSvnURI(remoteName, subPool, argLen);

                if (*verb)
                {
                    strbuf_t s = { buf + strlen(buf), argLen + 1 };
                    strbuf_cat(&s, verb, argLen);
                    verb = s.data - 1;
                    while (isspace(*verb) || *verb == '"') *verb-- = '\0';
                }
                command->proc(escapeURI(buf, subPool));
                return svn_pool_destroy(subPool), FS_EXEC_OK;
            }
            ++command;
        }

        {
            /* invalid command, display message box with command list */
            static const size_t errBufLen = 512;
            strbuf_t s = { buf = apr_palloc(subPool, errBufLen), errBufLen }; /* previous buf will be released by svn_pool_destroy */
            strbuf_init(&s, buf, errBufLen);
            strbuf_cat(&s, "Supported commands:\n\n", 21);
            command = commands;
            while (command->cmd.data)
            {
                strbuf_cat(&s, command->cmd.data, command->cmd.len);
                strbuf_cat(&s, "\t", 1);
                strbuf_cat(&s, command->helpParam, strlen(command->helpParam));
                strbuf_cat(&s, "\t", 1);
                strbuf_cat(&s, command->helpText, strlen(command->helpText));
                strbuf_cat(&s, "\n", 1);
                ++command;
            }
            strbuf_cat(&s, "\n\nIf the parameter is omitted, the current directory is assumed.", 64);
            MessageBox(mainWin, buf, "Subversion Plugin", MB_OK | MB_ICONINFORMATION);
        }
    }
    return svn_pool_destroy(subPool), FS_EXEC_ERROR;
}

/*--------------------------------------------------------------------------*/
int __stdcall FsExtractCustomIcon(char *remoteName, int extractFlags, HICON *icon)
{
    if (remoteName && *remoteName++ == '\\' )
    {
        if (!strcmp(remoteName, EditLocationsTitle.data))
        {
            *icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EDIT_LOCATIONS_ICON));
            return FS_ICON_EXTRACTED;
        }
    }
    return FS_ICON_USEDEFAULT;
}

/*--------------------------------------------------------------------------*/
void __stdcall FsContentPluginUnloading(void)
{
    freeLocationsAndSnapshots();
    if (Subversion.pool)
    {
        svn_pool_destroy(Subversion.pool);
        apr_terminate();
    }
}

/*--------------------------------------------------------------------------*/
void __stdcall FsSetDefaultParams(FsDefaultParamStruct *dps)
{
    const char *p = dps->DefaultIniName + strlen(dps->DefaultIniName);
    while (*p != '\\') --p;
    ++p;
    Config.configFilePath.len = p - dps->DefaultIniName + ConfigFileName.len + 1;
    Config.configFilePath.data = apr_palloc(Subversion.pool, Config.configFilePath.len);
    memcpy(Config.configFilePath.data, dps->DefaultIniName, p - dps->DefaultIniName);
    memcpy(Config.configFilePath.data + (p - dps->DefaultIniName), ConfigFileName.data, ConfigFileName.len + 1);
    loadConfig();
}

/*--------------------------------------------------------------------------*/
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        hInstance = hModule;
    }
    return TRUE;
}

/*--------------------------------------------------------------------------*/
static char *escapeURI(const char *uri, apr_pool_t *pool)
{
   const size_t uriLen = strlen(uri);

   if (uriLen)
   {
      const char *s = uri;
      char *dst = apr_palloc(pool, uriLen * 3 + 1);
      char *d = dst;

      while (*s)
      {
         if (!strchr(" %<>\"{}|\\^`#;?[]", *s))
         {
            *d++ = *s;
         }
         else
         {
            static const char *hexchars  = "0123456789ABCDEF";
            *d++ = '%';
            *d++ = hexchars[*s >> 4];
            *d++ = hexchars[*s & 0xf];
         }

         ++s;
      }
      *d = '\0';
      return dst;
   }
   return NULL;
}

/*--------------------------------------------------------------------------*/
static svn_error_t* list_func(Snapshot *snapshot,
                              const char *path,
                              const svn_dirent_t  *dirent,
                              const svn_lock_t  *lock,
                              const char *abs_path,
                              apr_pool_t *pool)
{
    if (*path)
    {
        SVNObject *obj = malloc(sizeof(SVNObject));
        memcpy(&obj->dirent, dirent, sizeof(*dirent));
        obj->dirent.last_author = strdup(obj->dirent.last_author);
        obj->name = strdup(path);
        obj->next = snapshot->entries;
        snapshot->entries = obj;
    }

    return 0;
}

/*--------------------------------------------------------------------------*/
static svn_error_t *querySnapshot(Snapshot *snapshot, const char *path)
{
    const size_t pathLen = strlen(path);
    Location *loc = Config.locations;
    while (loc)
    {
        const int minLen = min(loc->title.len, pathLen);
        if (!strncmp(path, loc->title.data, minLen))
        {
            /* found it, fetch data from SVN */
            apr_pool_t *subPool = svn_pool_create(Subversion.pool);
            svn_error_t *err;
            svn_opt_revision_t revision;
            size_t subPathLen = strlen(path + minLen);
            char *buf = apr_palloc(subPool, loc->url.len + subPathLen + 1);
            strbuf_t s = { buf, loc->url.len + subPathLen + 1 };

            snapshot->location = loc;
            snapshot->entries = NULL;
            revision.kind = svn_opt_revision_head;
            strbuf_cat(&s, loc->url.data, loc->url.len);
            if (subPathLen)
            {
                /* nested location */
                strbuf_cat(&s, path  + minLen, subPathLen);
                slashify(buf + loc->url.len);
                /* trim trailing slashes, SVN doesn't like those */
                while (s.data[-1] == '/')
                {
                    *(--s.data) = '\0';
                    ++s.size;
                    --subPathLen;
                }
            }
            err = svn_client_list2(escapeURI(buf, subPool), &revision, &revision, svn_depth_immediates, SVN_DIRENT_CREATED_REV | SVN_DIRENT_KIND | SVN_DIRENT_LAST_AUTHOR | SVN_DIRENT_SIZE | SVN_DIRENT_TIME, FALSE, (svn_client_list_func_t) list_func, snapshot, Subversion.ctx, subPool);
            svn_pool_destroy(subPool);
            if (!err)
            {
                snapshot->subPath.data = malloc(subPathLen + 1);
                snapshot->subPath.len = subPathLen;
                memcpy(snapshot->subPath.data, path + minLen, snapshot->subPath.len + 1);

                snapshot->current = snapshot->entries;
            }
            return err;
        }
        loc = loc->next;
    }

    return svn_error_create(SVN_ERR_BAD_URL, NULL, "Unknown Location");
}

/*--------------------------------------------------------------------------*/
static void try(svn_error_t *retVal)
{
    if ((Subversion.error = retVal))
    {
        MessageBox(NULL, Subversion.error->message, "Error", MB_OK);
    }
}

/*--------------------------------------------------------------------------*/
static int initSvn()
{
    if (apr_initialize() != APR_SUCCESS)
    {
        MessageBox(NULL, "apr_initialize failed!", NULL, MB_OK | MB_ICONERROR);
        return -1;
    }
    Subversion.pool = svn_pool_create(NULL);
    Subversion.error = svn_fs_initialize(Subversion.pool);
    try(svn_client_create_context(&Subversion.ctx, Subversion.pool));
    try(svn_config_get_config(&(Subversion.ctx->config), NULL, Subversion.pool));

    /* Make the client_ctx capable of authenticating users */
    {
        svn_auth_provider_object_t *provider;
        apr_array_header_t *providers = apr_array_make(Subversion.pool, 4, sizeof (svn_auth_provider_object_t *));

        svn_auth_get_simple_prompt_provider(&provider, promptCallback, NULL, /* baton */ 2, /* retry limit */ Subversion.pool);
        APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

        svn_auth_get_username_prompt_provider(&provider, promptCallbackUsername, NULL, /* baton */ 2, /* retry limit */ Subversion.pool);
        APR_ARRAY_PUSH (providers, svn_auth_provider_object_t *) = provider;

        /* Register the auth-providers into the context's auth_baton. */
        svn_auth_open (&Subversion.ctx->auth_baton, providers, Subversion.pool);
    }
    return 0;
}

/*--------------------------------------------------------------------------*/
static void loadConfig()
{
    char buf[1024];
    strbuf_t s = { buf, sizeof(buf) };
    FILE *f;

    if ((f = fopen(Config.configFilePath.data, "r")))
    {
        freeLocationsAndSnapshots();

        while (fgets(buf, sizeof(buf), f))
        {
            const char *p = buf, *left;

            while (isspace(*p)) ++p;
            if (*p == '#')
            {
                continue;
            }
            left = p;
            while (*p && *p != '\\' && *p != '=') ++p;
            if (*p == '\\')
            {
                /* Backslashes are disallowed */
                continue;
            }
            if (*p == '=')
            {
                const char *equals = p;
                Location *loc = malloc(sizeof(Location));

                while ((p > left) && isspace(p[-1])) --p;
                if (p > left)
                {
                    loc->title.data = malloc(p - left + 1);
                    memcpy(loc->title.data, left, p - left);
                    loc->title.data[p - left] = '\0';
                    loc->title.len = p - left;
                }
                else
                {
                    loc->title.data = NULL;
                    loc->title.len = 0;
                }

                p = equals + 1;
                while (*p && isspace(*p)) ++p;
                left = p;
                while (*p && *p != '\n') ++p;
                if (p > left)
                {
                    loc->url.data = malloc(p - left + 1);
                    memcpy(loc->url.data, left, p - left);
                    loc->url.data[p - left] = '\0';
                    loc->url.len = p - left;
                }
                else
                {
                    /* malformed */
                    free(loc->title.data);
                    free(loc);
                    continue;
                }

                loc->next = Config.locations;
                Config.locations = loc;
            }
        }

        fclose(f);
    }
    else if (f = fopen(buf, "w"))
    {
        static const char *defaultIniContents = "# svn_wfx configuration file. Layout:\n"
                                                "# title = svn_url\n"
                                                "# title may contain any character except Backslash (\\)\n"
                                                "# Lines starting with # or malformed lines are ignored.\n"
                                                "# Awesome Repository = svn://localhost/awesome\n\n";
        fprintf(f, defaultIniContents);
        fclose(f);
    }
    else
    {
        MessageBox(NULL, "Unable to access configuration file!", NULL, MB_OK | MB_ICONERROR);
    }
}

/*--------------------------------------------------------------------------*/
static void getSvnNode(SVNObject *obj, WIN32_FIND_DATA *findData)
{
    {
        strbuf_t s = { findData->cFileName, sizeof(findData->cFileName) };
        strbuf_cat(&s, obj->name, strlen(obj->name));
    }

    findData->dwFileAttributes = FILE_ATTRIBUTE_READONLY;

    {
        const LONGLONG tmpLL = (obj->dirent.time + APR_TIME_C(11644473600000000)) * 10;
        findData->ftLastWriteTime.dwLowDateTime  = (DWORD) tmpLL;
        findData->ftLastWriteTime.dwHighDateTime = (DWORD) (tmpLL >> 32ll);
    }

    if (obj->dirent.kind == svn_node_dir)
    {
        findData->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        findData->nFileSizeLow  = 0;
        findData->nFileSizeHigh = 0;
    }
    else
    {
        findData->nFileSizeLow  = (DWORD) obj->dirent.size;
        findData->nFileSizeHigh = (DWORD) (obj->dirent.size >> 32ll);
    }
}

/*--------------------------------------------------------------------------*/
static void replaceAll(char *str, char oldVal, char newVal)
{
    while (*str)
    {
        if (*str == oldVal) *str = newVal;
        ++str;
    }
}

/*--------------------------------------------------------------------------*/
static __inline void slashify(char *str)
{
    replaceAll(str, '\\', '/');
}

/*--------------------------------------------------------------------------*/
static void destroySnapshot(Snapshot *snapshot)
{
    if (snapshot)
    {
        SVNObject *obj = snapshot->entries;
        SVNObject *oldObj;
        free(snapshot->subPath.data);
        snapshot->subPath.data = NULL;
        snapshot->entries = NULL;
        while (obj)
        {
            free(obj->name);
            free((char*) obj->dirent.last_author);
            oldObj = obj;
            obj = obj->next;
            free(oldObj);
        }
    }
}

/*--------------------------------------------------------------------------*/
static void freeLocationsAndSnapshots()
{
    Location *loc = Config.locations;
    Location *oldLoc;
    while (loc)
    {
        free(loc->title.data);
        free(loc->url.data);
        oldLoc = loc;
        loc = loc->next;
        free(oldLoc);
    }
    Config.locations = NULL;
    destroySnapshot(&cachedSnapshot);
}

/*--------------------------------------------------------------------------*/
static char *remoteNameToSvnURI(char *remoteName, apr_pool_t *pool, size_t overAllocate)
{
    const Location *loc = Config.locations;
    size_t remoteNameLen = strlen(remoteName);
    while (loc)
    {
        if (!memcmp(remoteName, loc->title.data, min(remoteNameLen, loc->title.len)))
        {
            char *result;
            remoteName += loc->title.len;
            remoteNameLen -= loc->title.len;
            result = apr_palloc(pool, remoteNameLen + loc->url.len + 1 + overAllocate);
            slashify(remoteName);
            memcpy(result, loc->url.data, loc->url.len);
            memcpy(result + loc->url.len, remoteName, remoteNameLen + 1);
            return result;
        }

        loc = loc->next;
    }
    return 0;
}

/*--------------------------------------------------------------------------*/
static void displayErrorMessage(const char *msg)
{
    MessageBox(NULL, msg, NULL, MB_OK | MB_ICONERROR);
}

/*--------------------------------------------------------------------------*/
static svn_error_t *promptLine(const char *prompt,
                               char *buffer,
                               size_t max,
                               RequestRqType requestType)
{
    return Plugin.request(Plugin.id, requestType, NULL /* customTitle */, prompt, buffer, max) ? SVN_NO_ERROR : svn_error_create(SVN_ERR_CANCELLED, NULL, NULL);
}

/*--------------------------------------------------------------------------*/
static svn_error_t *promptCallback(svn_auth_cred_simple_t **cred,
                                   void *baton,
                                   const char *realm,
                                   const char *username,
                                   svn_boolean_t may_save,
                                   apr_pool_t *pool)
{
    svn_auth_cred_simple_t *ret = apr_pcalloc (pool, sizeof (*ret));
    char buf[1024];

    if (username)
    {
        ret->username = apr_pstrdup(pool, username);
    }
    else
    {
        DWORD bufSize = sizeof(buf);
        *buf = '\0';
        GetUserName(buf, &bufSize);
        SVN_ERR(promptLine("Username", buf, sizeof(buf), RQTYPE_USERNAME));
        ret->username = apr_pstrdup(pool, buf);
    }

    *buf = '\0';
    SVN_ERR(promptLine("Password", buf, sizeof(buf), RQTYPE_PASSWORD));
    ret->password = apr_pstrdup(pool, buf);

    *cred = ret;
    return SVN_NO_ERROR;
}

/*--------------------------------------------------------------------------*/
static svn_error_t *promptCallbackUsername(svn_auth_cred_username_t **cred,
                                           void *baton,
                                           const char *realm,
                                           svn_boolean_t may_save,
                                           apr_pool_t *pool)
{
    svn_auth_cred_username_t *ret = apr_pcalloc(pool, sizeof(*ret));
    char buf[1024];

    *buf = '\0';
    SVN_ERR(promptLine("Username", buf, sizeof(buf), RQTYPE_USERNAME));
    ret->username = apr_pstrdup(pool, buf);

    *cred = ret;
    return SVN_NO_ERROR;
}
