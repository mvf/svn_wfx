#ifndef SVN_WFX_STRBUF_H_INCLUDED
#define SVN_WFX_STRBUF_H_INCLUDED

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

#include <stdlib.h>

/** Zero-terminated string buffer for repeated concatenation into a char array */
typedef struct strbuf_t
{
    char *data;   /* Pointer to remainder of string */
    size_t size;  /* Remaining raw size in bytes, including the terminating zero */
} strbuf_t;

/** Convenience function that (re-)initializes a string buffer.
    Equivalent to just setting data and size yourself.
    @param str The string buffer to initialize.
    @param data The data pointer.
    @param size The size of the memory block pointed to by @a data.
    @return @a str */
extern strbuf_t *strbuf_init(strbuf_t *str, char *data, size_t size);

/** Copies at most n bytes from src into dst->data, zero-terminates dst->data
    and adjusts dst->size.
    @param dst The string buffer to manipulate. After strbuf_cat returns,
               dst->data points to the new end of the string and dst->size
               contains the new remaining size. If the string is modified,
               it is also zero-terminated.
               If any of @a n, dst->data or dst->size are zero, no operation is
               performed.
    @param src Pointer to source data of at least @a n bytes size. This data is
               copied using memcpy and does not need to be zero-terminated. Any
               zeroes in @src 's contents are copied as-is.
    @param n The maximum amount of bytes to copy. If @a n >= dst->size, only
               dst->size - 1 bytes are copied.
    @return @a dst */
extern strbuf_t *strbuf_cat(strbuf_t *dst, const char *src, size_t n);

/** Advances the buffer by at most @a n bytes. Useful for letting 3rd party functions
    copy a zero-terminated string of known length into your buffer.
    @param str The string buffer to manipulate.
    @param n The amount of bytes to advance. If @a n >= dst->size, str->data will be
             set to the last character in the string, which may or may not be zero!
    @return @a str */
extern strbuf_t *strbuf_adv(strbuf_t *str, size_t n);

#endif /* !SVN_WFX_STRBUF_H_INCLUDED */

