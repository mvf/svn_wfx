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

#include "strbuf.h"

#include <string.h>

strbuf_t *strbuf_init(strbuf_t *str, char *data, size_t size)
{
    str->data = data;
    str->size = size;
    return str;
}

strbuf_t *strbuf_cat(strbuf_t *dst, const char *src, size_t n)
{
    const size_t copyLen = n < dst->size ? n : dst->size - 1;
    if (dst->size)
    {
        if (copyLen)
        {
            memcpy(dst->data, src, copyLen);
            dst->data += copyLen;
            dst->size -= copyLen;
        }
        *dst->data = '\0';
    }
    return dst;
}

strbuf_t *strbuf_adv(strbuf_t *str, size_t n)
{
    const size_t advLen = n < str->size ? n : str->size - 1;
    if (str->size && advLen)
    {
        str->data += advLen;
        str->size -= advLen;
    }
    return str;
}

