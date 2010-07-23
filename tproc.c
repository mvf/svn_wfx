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

#include "tproc.h"

#include "strbuf.h"

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
** Prototypes
*/
static void tproc_unary_cmd(const char *command, const char *paramName, const char *path, WORD showCmd);

/*
** Globals
*/
static struct
{
    STARTUPINFO startupInfo;
    error_handler_t errorHandler;
    size_t tprocPathLen;
    char tprocPath[MAX_PATH];
} Global = { 0 };

/*--------------------------------------------------------------------------*/
void tproc_init(error_handler_t errorHandler)
{
    HKEY key;
    GetStartupInfo(&Global.startupInfo);
    Global.errorHandler = errorHandler;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\TortoiseSVN", 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        DWORD size = sizeof(Global.tprocPath);
        if (RegQueryValueEx(key, "ProcPath", NULL, NULL, Global.tprocPath, &size) == ERROR_SUCCESS)
        {
            Global.tprocPathLen = strlen(Global.tprocPath);
            Global.tprocPath[Global.tprocPathLen++] = ' ';
            Global.tprocPath[Global.tprocPathLen] = '\0';
        }
        RegCloseKey(key);
    }
}

/*--------------------------------------------------------------------------*/
void tproc_checkout(const char *url)
{
    tproc_unary_cmd("checkout", "url", url, SW_RESTORE);
}

/*--------------------------------------------------------------------------*/
void tproc_blame(const char *url)
{
    tproc_unary_cmd("blame", "path", url, SW_RESTORE);
}

/*--------------------------------------------------------------------------*/
void tproc_log(const char *url)
{
    tproc_unary_cmd("log", "path", url, SW_MAXIMIZE);
}

/*--------------------------------------------------------------------------*/
void tproc_props(const char *url)
{
    tproc_unary_cmd("properties", "path", url, SW_RESTORE);
}

/*--------------------------------------------------------------------------*/
void tproc_repobrowser(const char *url)
{
    tproc_unary_cmd("repobrowser", "path", url, SW_MAXIMIZE);
}

/*--------------------------------------------------------------------------*/
void tproc_revgraph(const char *url)
{
    tproc_unary_cmd("revisiongraph", "path", url, SW_MAXIMIZE);
}

/*--------------------------------------------------------------------------*/
static void tproc_unary_cmd(const char *command, const char *paramName, const char *path, WORD showCmd)
{
    if (*Global.tprocPath)
    {
        PROCESS_INFORMATION pi;
        char buf[1024];
        strbuf_t s = { buf, sizeof(buf) };
        strbuf_cat(&s, Global.tprocPath, Global.tprocPathLen);
        Global.startupInfo.wShowWindow = showCmd;
        strbuf_cat(&s, "/command:", 9);
        strbuf_cat(&s, command, strlen(command));
        strbuf_cat(&s, " /", 2);
        strbuf_cat(&s, paramName, strlen(paramName));
        strbuf_cat(&s, ":\"", 2);
        strbuf_cat(&s, path, strlen(path));
        strbuf_cat(&s, "\"", 1);
        CreateProcess(NULL, buf, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &Global.startupInfo, &pi);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    else
    {
        Global.errorHandler("TortoiseSVN was not found.");
    }
}
