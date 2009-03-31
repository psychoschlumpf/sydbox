/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ali Polatel
 * Based in part upon coreutils' lib/canonicalize.c which is:
 *  Copyright (C) 1996-2008 Free Software Foundation, Inc.
 *
 * This file is part of the sydbox sandbox tool. sydbox is free software;
 * you can redistribute it and/or modify it under the terms of the GNU General
 * Public License version 2, as published by the Free Software Foundation.
 *
 * sydbox is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <libgen.h>
#if defined(basename)
#undef basename // get the GNU version from string.h
#endif
#include <string.h>

#include <stddef.h>
#include <sys/stat.h>

#include <glib.h>

#include "defs.h"
#include "path.h"
#include "wrappers.h"

#ifndef __set_errno
# define __set_errno(v) errno = (v)
#endif

#ifndef MAXSYMLINKS
# define MAXSYMLINKS 256
#endif

// dirname wrapper which doesn't modify its argument
char *edirname(const char *path) {
    char *pathc = g_strdup (path);
    char *dname = dirname(pathc);
    char *dnamec = g_strdup (dname);
    g_free (pathc);
    return dnamec;
}

char *ebasename(const char *path) {
    return basename(path);
}

// readlink that allocates the string itself and appends a zero at the end
char *ereadlink(const char *path) {
    char *buf;
    long nrequested, nwritten;

    buf = NULL;
    nrequested = 32;
    for (;;) {
        buf = g_realloc (buf, nrequested);
        nwritten = readlink(path, buf, nrequested);
        if (0 > nwritten) {
            g_free (buf);
            return NULL;
        }
        else if (nrequested > nwritten)
            break;
        else
            nrequested *= 2;
    }
    buf[nwritten] = '\0';
    return buf;
}

/* Return the canonical absolute name of file NAME.  A canonical name
   does not contain any `.', `..' components nor any repeated file name
   separators ('/') or symlinks.  Whether components must exist
   or not depends on canonicalize mode.  The result is malloc'd.  */

char *canonicalize_filename_mode(const char *name, canonicalize_mode_t can_mode,
        bool resolve, const char *cwd) {
    int readlinks = 0;
    char *rname, *dest, *extra_buf = NULL;
    char const *start;
    char const *end;
    char const *rname_limit;
    size_t extra_len = 0;

    if (name == NULL) {
        __set_errno(EINVAL);
        return NULL;
    }

    if (name[0] == '\0') {
        __set_errno(ENOENT);
        return NULL;
    }

    if (name[0] != '/') {
        rname = g_strdup (cwd);
        dest = strchr(rname, '\0');
        if (dest - rname < PATH_MAX) {
            char *p = g_realloc (rname, PATH_MAX);
            dest = p + (dest - rname);
            rname = p;
            rname_limit = rname + PATH_MAX;
        }
        else
            rname_limit = dest;
    }
    else {
        cwd = NULL;
        rname = g_malloc (PATH_MAX);
        rname_limit = rname + PATH_MAX;
        rname[0] = '/';
        dest = rname + 1;
    }

    for (start = end = name; *start; start = end) {
        /* Skip sequence of multiple file name separators.  */
        while (*start == '/')
            ++start;

        /* Find end of component.  */
        for (end = start; *end && *end != '/'; ++end)
            /* Nothing.  */;

        if (end - start == 0)
            break;
        else if (end - start == 1 && start[0] == '.')
            /* nothing */;
        else if (end - start == 2 && start[0] == '.' && start[1] == '.') {
            /* Back up to previous component, ignore if at root already.  */
            if (dest > rname + 1)
                while ((--dest)[-1] != '/');
        }
        else {
            struct stat st;

            if (dest[-1] != '/')
                *dest++ = '/';

            if (dest + (end - start) >= rname_limit) {
                ptrdiff_t dest_offset = dest - rname;
                size_t new_size = rname_limit - rname;

                if (end - start + 1 > PATH_MAX)
                    new_size += end - start + 1;
                else
                    new_size += PATH_MAX;
                rname = g_realloc (rname, new_size);
                rname_limit = rname + new_size;

                dest = rname + dest_offset;
            }

            dest = memcpy (dest, start, end - start);
            dest += end - start;
            *dest = '\0';

            int ret;
            if (NULL != cwd && 0 == strncmp(rname, cwd, strlen(cwd)))
                ret = lstat(rname + strlen(cwd) + 1, &st);
            else
                ret = lstat(rname, &st);
            if (0 != ret) {
                if (can_mode == CAN_EXISTING)
                    goto error;
                if (can_mode == CAN_ALL_BUT_LAST && *end)
                    goto error;
                st.st_mode = 0;
            }

            if (S_ISLNK (st.st_mode)) {
                char *buf;
                size_t n, len;

                if (!resolve)
                    continue;

                /* Protect against infinite loops */
                if (readlinks++ > MAXSYMLINKS) {
                    __set_errno(ELOOP);
                    goto error;
                }

                if (NULL != cwd && 0 == strncmp(rname, cwd, strlen(cwd)))
                    buf = ereadlink(rname + strlen(cwd) + 1);
                else
                    buf = ereadlink(rname);
                if (!buf)
                    goto error;

                n = strlen (buf);
                len = strlen (end);

                if (!extra_len) {
                    extra_len =
                        ((n + len + 1) > PATH_MAX) ? (n + len + 1) : PATH_MAX;
                    extra_buf = g_malloc (extra_len);
                }
                else if ((n + len + 1) > extra_len) {
                    extra_len = n + len + 1;
                    extra_buf = g_realloc (extra_buf, extra_len);
                }

                /* Careful here, end may be a pointer into extra_buf... */
                memmove (&extra_buf[n], end, len + 1);
                name = end = memcpy (extra_buf, buf, n);

                if (buf[0] == '/')
                    dest = rname + 1;   /* It's an absolute symlink */
                else
                    /* Back up to previous component, ignore if at root already: */
                    if (dest > rname + 1)
                        while ((--dest)[-1] != '/');

                g_free (buf);
            }
            else {
                if (!S_ISDIR (st.st_mode) && *end) {
                    __set_errno(ENOTDIR);
                    goto error;
                }
            }
        }
    }
    if (dest > rname + 1 && dest[-1] == '/')
        --dest;
    *dest = '\0';

    g_free (extra_buf);
    return rname;

error:
  g_free (extra_buf);
  g_free (rname);
  return NULL;
}