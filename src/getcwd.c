/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 1992-1997 Paul Falstad
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "log.h"
#include "getcwd.h"

/* chdir with arbitrary long pathname.  Returns 0 on success, -1 on normal *
 * failure and -2 when chdir failed and the current directory is lost.  */

int echdir(char *dir) {
    char *s;
    int currdir = -2;

    for (;;) {
        if (!*dir || chdir(dir) == 0) {
            return 0;
        }
        if ((errno != ENAMETOOLONG && errno != ENOMEM) ||
                strlen(dir) < PATH_MAX)
            break;
        for (s = dir + PATH_MAX - 1; s > dir && *s != '/'; s--)
            ;
        if (s == dir)
            break;
        *s = '\0';
        if (chdir(dir) < 0) {
            *s = '/';
            break;
        }
        *s = '/';
        while (*++s == '/')
            ;
        dir = s;
    }
    return currdir == -2 ? -1 : -2;
}

char *egetcwd(void) {
    char nbuf[PATH_MAX+3];
    char *buf;
    int bufsiz, pos;
    struct stat sbuf;
    ino_t pino;
    dev_t pdev;
    struct dirent *de;
    DIR *dir;
    dev_t dev;
    ino_t ino;
    int len;
    int save_errno;

    /* First try getcwd() */
    buf = getcwd(NULL, 0);
    if (NULL != buf)
        return buf;
    else if (ENAMETOOLONG != errno)
        return NULL;

    /* Next try stat()'ing and chdir()'ing up */
    bufsiz = PATH_MAX;
    buf = g_malloc0 (bufsiz);
    pos = bufsiz - 1;
    buf[pos] = '\0';
    strcpy(nbuf, "../");
    if (0 > stat(".", &sbuf)) {
        g_free (buf);
        return NULL;
    }

    /* Record the initial inode and device */
    pino = sbuf.st_ino;
    pdev = sbuf.st_dev;

    for (;;) {
        if (0 > stat("..", &sbuf))
            break;

        /* Inode and device of current directory */
        ino = pino;
        dev = pdev;
        /* Inode and device of current directory's parent */
        pino = sbuf.st_ino;
        pdev = sbuf.st_dev;

        /* If they're the same, we've reached the root directory. */
        if (ino == pino && dev == pdev) {
            if (!buf[pos])
                buf[--pos] = '/';
            char *s = g_strdup (buf + pos);
            g_free (buf);
            echdir(s);
            return s;
        }

        /* Search the parent for the current directory. */
        dir = opendir("..");
        if (NULL == dir) {
            save_errno = errno;
            g_debug ("opendir() failed: %s", strerror(errno));
            errno = save_errno;
            break;
        }

        while ((de = readdir(dir))) {
            char *fn = de->d_name;
            /* Ignore `.' and `..'. */
            if (fn[0] == '.' &&
                (fn[1] == '\0' ||
                 (fn[1] == '.' && fn[2] == '\0')))
                continue;
            if (dev != pdev || (ino_t) de->d_ino == ino) {
                /* Maybe found directory, need to check device & inode */
                strncpy(nbuf + 3, fn, PATH_MAX);
                lstat(nbuf, &sbuf);
                if (sbuf.st_dev == dev && sbuf.st_ino == ino)
                    break;
            }
        }
        closedir(dir);
        if (!de)
            break; /* Not found */
        len = strlen(nbuf + 2);
        pos -= len;
        while (pos <= 1) {
            char *temp;
            char *newbuf = g_malloc0 (2 * bufsiz);
            memcpy(newbuf + bufsiz, buf, bufsiz);
            temp = buf;
            buf = newbuf;
            g_free (temp);
            pos += bufsiz;
            bufsiz *= 2;
        }
        memcpy(buf + pos, nbuf + 2, len);

        if (0 > chdir(".."))
            break;
    }

    if (*buf) {
        g_debug ("changing current working directory to `%s'", buf + pos + 1);
        echdir(buf + pos + 1);
    }
    g_free (buf);
    return NULL;
}