#ifndef SVN_WFX_TPROC_H_INCLUDED
#define SVN_WFX_TPROC_H_INCLUDED

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

/** Error handler callback.
    @param msg The error message. */
typedef void (*error_handler_t)(const char *msg);

/** Initializes TortoiseProc.
    @param errorHandler The function to receive error messages. */
extern void tproc_init(error_handler_t errorHandler);

/** Opens the checkout dialog.
    @param url The subversion URL */
extern void tproc_checkout(const char *url);

/** Opens the blame dialog.
    @param url The subversion URL */
extern void tproc_blame(const char *url);

/** Opens the log dialog.
    @param url The subversion URL */
extern void tproc_log(const char *url);

/** Opens the SVN properties dialog.
    @param url The subversion URL */
extern void tproc_props(const char *url);

/** Opens the repository browser.
    @param url The subversion URL */
extern void tproc_repobrowser(const char *url);

/** Opens the revision graph dialog.
    @param url The subversion URL */
extern void tproc_revgraph(const char *url);

#endif /* !SVN_WFX_TPROC_H_INCLUDED */

