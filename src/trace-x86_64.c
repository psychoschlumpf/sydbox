/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ali Polatel <polatel@gmail.com>
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

#include <assert.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>

#include "trace.h"
#include "trace-util.h"

#define ORIG_ACCUM      (8 * ORIG_RAX)
#define ACCUM           (8 * RAX)
static const long syscall_args[2][MAX_ARGS] = {
    {8 * RBX, 8 * RCX, 8 * RDX, 8 * RSI, 8 * RDI, 8 * RBP},
    {8 * RDI, 8 * RSI, 8 * RDX, 8 * R10, 8 * R8, 8 * R9}
};

int trace_personality(pid_t pid)
{
    long cs;

    /* Check CS register value,
     * On x86-64 linux this is:
     *  0x33    for long mode (64 bit)
     *  0x23    for compatibility mode (32 bit)
     */
    if (0 > upeek(pid, 8 * CS, &cs))
        return -1;

    switch (cs) {
        case 0x33:
            return 1;
        case 0x23:
            return 0;
        default:
            g_assert_not_reached();
            break;
    }
}

int trace_get_syscall(pid_t pid, long *scno)
{
    int save_errno;

    if (G_UNLIKELY(0 > upeek(pid, ORIG_ACCUM, scno))) {
        save_errno = errno;
        g_info("failed to get syscall number for child %i: %s", pid, g_strerror(errno));
        errno = save_errno;
        return -1;
    }

    return 0;
}

int trace_set_syscall(pid_t pid, long scno)
{
    int save_errno;

    if (G_UNLIKELY(0 > ptrace(PTRACE_POKEUSER, pid, ORIG_ACCUM, scno))) {
        save_errno = errno;
        g_info("failed to set syscall number to %ld for child %i: %s", scno, pid, g_strerror(errno));
        errno = save_errno;
        return -1;
    }

    return 0;
}

int trace_get_return(pid_t pid, long *res)
{
    int save_errno;

    if (G_UNLIKELY(0 > upeek(pid, ACCUM, res))) {
        save_errno = errno;
        g_info("failed to get return value for child %i: %s", pid, g_strerror (errno));
        errno = save_errno;
        return -1;
    }

    return 0;
}

int trace_set_return(pid_t pid, long val)
{
    int save_errno;

    if (G_UNLIKELY(0 != ptrace(PTRACE_POKEUSER, pid, ACCUM, val))) {
        save_errno = errno;
        g_info("ptrace(PTRACE_POKEUSER,%i,ACCUM,%ld) failed: %s", pid, val, g_strerror(errno));
        errno = save_errno;
        return -1;
    }

    return 0;
}

int trace_get_arg(pid_t pid, int personality, int arg, long *res)
{
    int save_errno;

    assert(arg >= 0 && arg < MAX_ARGS);

    if (G_UNLIKELY(0 > upeek(pid, syscall_args[personality][arg], res))) {
        save_errno = errno;
        g_info("failed to get argument %d for child %i: %s", arg, pid, strerror(errno));
        errno = save_errno;
        return -1;
    }

    return 0;
}

char *trace_get_path(pid_t pid, int personality, int arg)
{
    int save_errno;
    long addr = 0;

    assert(arg >= 0 && arg < MAX_ARGS);

    if (G_UNLIKELY(0 > upeek(pid, syscall_args[personality][arg], &addr))) {
        save_errno = errno;
        g_info("failed to get address of argument %d: %s", arg, g_strerror(errno));
        errno = save_errno;
        return NULL;
    }

    char *buf = NULL;
    long len = PATH_MAX;
    for (;;) {
        buf = g_realloc(buf, len * sizeof(char));
        memset(buf, 0, len * sizeof(char));
        if (G_UNLIKELY(0 > umovestr(pid, addr, buf, len))) {
            g_free(buf);
            return NULL;
        }
        else if ('\0' == buf[len - 1])
            break;
        else
            len *= 2;
    }
    return buf;
}

int trace_set_path(pid_t pid, int personality, int arg, const char *src, size_t len)
{
    int n, m, save_errno;
    long addr = 0;
    union {
        long val;
        char x[sizeof(long)];
    } u;

    assert(arg >= 0 && arg < MAX_ARGS);

    if (G_UNLIKELY(0 > upeek(pid, syscall_args[personality][arg], &addr))) {
        save_errno = errno;
        g_info("failed to get address of argument %d: %s", arg, g_strerror(errno));
        errno = save_errno;
        return -1;
    }

    n = 0;
    m = len / sizeof(long);

    while (n < m) {
        memcpy(u.x, src, sizeof(long));
        if (G_UNLIKELY(0 > ptrace(PTRACE_POKEDATA, pid, addr + n * ADDR_MUL, u.val))) {
            save_errno = errno;
            g_info("failed to set argument %d to \"%s\" for child %i: %s", arg, src, pid, g_strerror(errno));
            errno = save_errno;
            return -1;
        }
        ++n;
        src += sizeof(long);
    }

    m = len % sizeof(long);
    if (0 != m) {
        errno = 0;
        u.val = ptrace(PTRACE_PEEKDATA, pid, addr + n * ADDR_MUL, 0);
        if (G_UNLIKELY(0 != errno)) {
            save_errno = errno;
            g_info("failed to set argument %d to \"%s\" for child %i: %s", arg, src, pid, g_strerror(errno));
            errno = save_errno;
            return -1;
        }
        memcpy(u.x, src, m);
        if (G_UNLIKELY(0 > ptrace(PTRACE_POKEDATA, pid, addr + n * ADDR_MUL, u.val))) {
            save_errno = errno;
            g_info("failed to set argument %d to \"%s\" for child %i: %s", arg, src, pid, g_strerror(errno));
            errno = save_errno;
            return -1;
        }
    }
    return 0;
}

int trace_fake_stat(pid_t pid, int personality)
{
    int n, m, save_errno;
    long addr = 0;
    union {
        long val;
        char x[sizeof(long)];
    } u;
    struct stat fakebuf;

    if (G_UNLIKELY(0 > upeek(pid, syscall_args[personality][1], &addr))) {
        save_errno = errno;
        g_info("failed to get address of argument %d: %s", 1, g_strerror(errno));
        errno = save_errno;
        return -1;
    }

    memset(&fakebuf, 0, sizeof(struct stat));
    fakebuf.st_mode = S_IFDIR;
    fakebuf.st_uid  = 0;
    fakebuf.st_gid  = 0;

    long *fakeptr = (long *) &fakebuf;
    n = 0;
    m = sizeof(struct stat) / sizeof(long);
    while (n < m) {
        memcpy(u.x, fakeptr, sizeof(long));
        if (0 > ptrace(PTRACE_POKEDATA, pid, addr + n * ADDR_MUL, u.val)) {
            save_errno = errno;
            g_info("failed to set argument 1 to %p for child %i: %s", (void *) fakeptr, pid, g_strerror(errno));
            errno = save_errno;
            return -1;
        }
        ++n;
        ++fakeptr;
    }

    m = sizeof(struct stat) % sizeof(long);
    if (0 != m) {
        memcpy(u.x, fakeptr, m);
        if (G_UNLIKELY(0 > ptrace(PTRACE_POKEDATA, pid, addr + n * ADDR_MUL, u.val))) {
            save_errno = errno;
            g_info("failed to set argument 1 to %p for child %i: %s", (void *) fakeptr, pid, g_strerror(errno));
            errno = save_errno;
            return -1;
        }
    }
    return 0;
}
