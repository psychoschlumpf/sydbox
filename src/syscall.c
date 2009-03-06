/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ali Polatel
 * Based in part upon catbox which is:
 *  Copyright (c) 2006-2007 TUBITAK/UEKAE
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

#define _ATFILE_SOURCE // AT_FDCWD

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/stat.h>

#include "defs.h"

// System call dispatch flags
#define RETURNS_FD      (1 << 0) // The function returns a file descriptor
#define OPEN_MODE       (1 << 1) // Check the mode argument of open()
#define OPEN_MODE_AT    (1 << 2) // Check the mode argument of openat()
#define ACCESS_MODE     (1 << 3) // Check the mode argument of access()
#define ACCESS_MODE_AT  (1 << 4) // Check the mode argument of faccessat()
#define CHECK_PATH      (1 << 5) // First argument should be a valid path
#define CHECK_PATH2     (1 << 6) // Second argument should be a valid path
#define CHECK_PATH_AT   (1 << 7) // CHECK_PATH for at suffixed functions
#define CHECK_PATH_AT2  (1 << 8) // CHECK_PATH2 for at suffixed functions
#define DONT_RESOLV     (1 << 9) // Don't resolve symlinks
#define CAN_CREAT       (1 << 10) // The system call can create the first path if it doesn't exist
#define CAN_CREAT2      (1 << 11) // The system call can create the second path if it doesn't exist
#define CAN_CREAT_AT    (1 << 12) // CAN_CREAT for at suffixed functions
#define CAN_CREAT_AT2   (1 << 13) // CAN_CREAT_AT2 for at suffixed functions
#define MAGIC_OPEN      (1 << 14) // Check if the open() call is magic
#define MAGIC_STAT      (1 << 15) // Check if the stat() call is magic
#define NET_CALL        (1 << 16) // Allowing the system call depends on the net flag

static const struct syscall_name {
    int no;
    const char *name;
} sysnames[] = {
#include "syscallent.h"
{-1,    NULL}
};

// System call dispatch table
static const struct syscall_def syscalls[] = {
    {__NR_chmod,        CHECK_PATH},
    {__NR_chown,        CHECK_PATH},
#if defined(__NR_chown32)
    {__NR_chown32,      CHECK_PATH},
#endif
    {__NR_open,         CHECK_PATH | RETURNS_FD | OPEN_MODE | MAGIC_OPEN},
    {__NR_creat,        CHECK_PATH | CAN_CREAT | RETURNS_FD},
    {__NR_stat,         MAGIC_STAT},
#if defined(__NR_stat64)
    {__NR_stat64,       MAGIC_STAT},
#endif
    {__NR_lchown,       CHECK_PATH | DONT_RESOLV},
#if defined(__NR_lchown32)
    {__NR_lchown32,     CHECK_PATH | DONT_RESOLV},
#endif
    {__NR_link,         CHECK_PATH | CHECK_PATH2 | CAN_CREAT2},
    {__NR_mkdir,        CHECK_PATH | CAN_CREAT},
    {__NR_mknod,        CHECK_PATH | CAN_CREAT},
    {__NR_access,       CHECK_PATH | ACCESS_MODE},
    {__NR_rename,       CHECK_PATH | CHECK_PATH2 | CAN_CREAT2},
    {__NR_rmdir,        CHECK_PATH},
    {__NR_symlink,      CHECK_PATH2 | CAN_CREAT2 | DONT_RESOLV},
    {__NR_truncate,     CHECK_PATH},
#if defined(__NR_truncate64)
    {__NR_truncate64,   CHECK_PATH},
#endif
    {__NR_mount,        CHECK_PATH2},
#if defined(__NR_umount)
    {__NR_umount,       CHECK_PATH},
#endif
    {__NR_umount2,      CHECK_PATH},
    {__NR_utime,        CHECK_PATH},
    {__NR_unlink,       CHECK_PATH},
    {__NR_openat,       CHECK_PATH_AT | OPEN_MODE_AT | RETURNS_FD},
    {__NR_mkdirat,      CHECK_PATH_AT | CAN_CREAT_AT},
    {__NR_mknodat,      CHECK_PATH_AT | CAN_CREAT_AT},
    {__NR_fchownat,     CHECK_PATH_AT},
    {__NR_unlinkat,     CHECK_PATH_AT},
    {__NR_renameat,     CHECK_PATH_AT | CHECK_PATH_AT2 | CAN_CREAT_AT2},
    {__NR_linkat,       CHECK_PATH_AT | CHECK_PATH_AT2 | CAN_CREAT_AT2},
    {__NR_symlinkat,    CHECK_PATH_AT | CHECK_PATH_AT2 | CAN_CREAT_AT2 | DONT_RESOLV},
    {__NR_fchmodat,     CHECK_PATH_AT},
    {__NR_faccessat,    CHECK_PATH_AT | ACCESS_MODE_AT},
#if defined(__NR_socketcall)
    {__NR_socketcall,   NET_CALL},
#elif defined(__NR_socket)
    {__NR_socket,       NET_CALL},
#else
#error unsupported architecture
#endif
    {-1,                 0}
};

enum res_mode {
    RM_ERROR, // Error occured while checking mode
    RM_WRITE, // Write flags set in mode (O_WR, O_WRONLY or O_RDWR)
    RM_NOWRITE, // No write flags in mode
    RM_CREAT, // O_CREAT is in mode
};

static inline const char *syscall_get_name(int no) {
    for (int i = 0; sysnames[i].name != NULL; i++) {
        if (sysnames[i].no == no)
            return sysnames[i].name;
    }
    return NULL;
}

static enum res_syscall syscall_check_prefix(context_t *ctx, struct tchild *child,
        int arg, const struct syscall_def *sdef,
        const char *path, const char *rpath) {
    LOGD("Checking \"%s\" for write access", rpath);
    int allow_write = pathlist_check(&(ctx->write_prefixes), rpath);
    LOGD("Checking \"%s\" for predict access", rpath);
    int allow_predict = pathlist_check(&(ctx->predict_prefixes), rpath);

    char reason[PATH_MAX + 128];
    const char *sname;
    if (!allow_write && !allow_predict) {
        child->retval = -EPERM;
        if (0 == arg)
            strcpy(reason, "%s(\"%s\", ");
        else if (1 == arg)
            strcpy(reason, "%s(?, \"%s\", ");
        if (sdef->flags & ACCESS_MODE)
            strcat(reason, "O_WR)");
        else if (sdef->flags & OPEN_MODE || sdef->flags & OPEN_MODE_AT)
            strcat(reason, "O_WRONLY/O_RDWR)");
        else
            strcat(reason, "...)");
        sname = syscall_get_name(sdef->no);
        access_error(child->pid, reason, sname, path);
        return RS_DENY;
    }
    else if (!allow_write && allow_predict) {
        if (sdef->flags & RETURNS_FD) {
            LOGD("System call returns fd and its argument is under a predict path");
            LOGD("Changing the path argument to /dev/null");
            if (0 > trace_set_string(child->pid, arg, "/dev/null", 10))
                DIESOFT("Failed to set string: %s", strerror(errno));
            return RS_ALLOW;
        }
        else {
            child->retval = 0;
            return RS_DENY;
        }
    }

    if (ctx->paranoid && !(sdef->flags & DONT_RESOLV)) {
        /* Change the path argument with the resolved path to
        * prevent symlink races.
        */
        LOGV("Paranoia! System call has DONT_RESOLV unset, substituting path with resolved path");
        if (0 > trace_set_string(child->pid, arg, rpath, PATH_MAX)) {
            int save_errno = errno;
            LOGE("Failed to set string: %s", strerror(errno));
            errno = save_errno;
            return RS_ERROR;
        }
    }
    return RS_ALLOW;
}

static int syscall_process_pathat(pid_t pid, int arg, char *dest) {
    int save_errno;
    long dirfd;

    assert(1 == arg || 3 == arg);
    if (0 > trace_get_arg(pid, arg - 1, &dirfd)) {
        save_errno = errno;
        LOGE("Failed to get dirfd: %s", strerror(errno));
        errno = save_errno;
        return -1;
    }
    if (0 > trace_get_string(pid, arg, dest, PATH_MAX)) {
        save_errno = errno;
        LOGE("Failed to get string from argument %d: %s", arg, strerror(errno));
        errno = save_errno;
        return -1;
    }

    if (AT_FDCWD != dirfd && '/' != dest[0]) {
        int n;
        char dname[PATH_MAX], res_dname[PATH_MAX];

        snprintf(dname, PATH_MAX, "/proc/%i/fd/%ld", pid, dirfd);
        n = readlink(dname, res_dname, PATH_MAX - 1);
        if (0 > n)
            DIESOFT("readlink() failed for %s: %s", dname, strerror(errno));
        res_dname[n] = '\0';

        char *destc = xstrndup(dest, PATH_MAX);
        snprintf(dest, PATH_MAX, "%s/%s", res_dname, destc);
        free(destc);
    }
    return 0;
}

static enum res_mode syscall_checkmode_access(pid_t pid) {
    long mode;
    LOGD("Checking mode argument of access() for child %i", pid);
    if (0 > trace_get_arg(pid, 1, &mode)) {
        LOGE("Failed to get mode: %s", strerror(errno));
        return RM_ERROR;
    }

    if (mode & W_OK) {
        LOGD("W_OK found in mode");
        return RM_WRITE;
    }
    else {
        LOGD("W_OK not found in mode");
        return RM_NOWRITE;
    }
}

static enum res_mode syscall_checkmode_accessat(pid_t pid) {
    long mode;

    LOGD("Checking mode argument of faccessat() for child %i", pid);
    if (0 > trace_get_arg(pid, 2, &mode)) {
        LOGE("Failed to get mode: %s", strerror(errno));
        return RM_ERROR;
    }

    if (mode & W_OK) {
        LOGD("W_OK set");
        return RM_WRITE;
    }
    else {
        LOGD("W_OK not set");
        return RM_NOWRITE;
    }
}

static enum res_mode syscall_checkmode_open(pid_t pid) {
    long mode;

    LOGD("Checking mode argument of open() for child %i", pid);
    if (0 > trace_get_arg(pid, 1, &mode)) {
        LOGE("Failed to get mode: %s", strerror(errno));
        return RM_ERROR;
    }

    if (mode & O_CREAT) {
        LOGD("O_CREAT set");
        return RM_CREAT;
    }
    else if (mode & O_WRONLY || mode & O_RDWR) {
        LOGD("O_WRONLY or O_RDWR set");
        return RM_WRITE;
    }
    else {
        LOGD("Neither O_CREAT nor O_WRONLY or O_RDWR set");
        return RM_NOWRITE;
    }
}

static enum res_mode syscall_checkmode_openat(pid_t pid) {
    long mode;

    LOGD("Checking mode argument of openat() for child %i", pid);
    if (0 > trace_get_arg(pid, 2, &mode)) {
        LOGE("Failed to get mode: %s", strerror(errno));
        return RM_ERROR;
    }

    if (mode & O_CREAT) {
        LOGD("O_CREAT set");
        return RM_CREAT;
    }
    else if (mode & O_WRONLY || mode & O_RDWR) {
        LOGD("O_WRONLY or O_RDWR set");
        return RM_WRITE;
    }
    else {
        LOGD("Neither O_CREAT nor O_WRONLY or O_RDWR set");
        return RM_NOWRITE;
    }
}

static inline int syscall_can_creat(int arg, int flags) {
    if (0 == arg && flags & CAN_CREAT)
        return 1;
    else if (1 == arg && flags & CAN_CREAT2)
        return 1;
    else if (2 == arg && flags & CAN_CREAT_AT)
        return 1;
    else if (3 == arg && flags & CAN_CREAT_AT2)
        return 1;
    else
        return 0;
}

static enum res_syscall syscall_check_path(context_t *ctx, struct tchild *child, int arg,
        const struct syscall_def *sdef, const char *openpath) {
    int save_errno;
    char path[PATH_MAX];
    char *rpath = NULL;

    assert(0 <= arg || 3 >= arg);

    int ret = 0, check_ret = 0;
    if (sdef->flags & ACCESS_MODE) {
        check_ret = 1;
        ret = syscall_checkmode_access(child->pid);
    }
    else if (sdef->flags & ACCESS_MODE_AT) {
        check_ret = 1;
        ret = syscall_checkmode_accessat(child->pid);
    }
    else if (sdef->flags & OPEN_MODE) {
        check_ret = 1;
        ret = syscall_checkmode_open(child->pid);
    }
    else if (sdef->flags & OPEN_MODE_AT) {
        check_ret = 1;
        ret = syscall_checkmode_openat(child->pid);
    }

    if (check_ret) {
        if (RM_ERROR == ret) {
            save_errno = errno;
            LOGE("Failed to check mode: %s", strerror(errno));
            errno = save_errno;
            return RS_ERROR;
        }
        else if (RM_NOWRITE == ret) {
            LOGD("No write or create flags set, allowing access");
            return RS_ALLOW;
        }
    }

    if (sdef->flags & CHECK_PATH) {
        if (sdef->flags & MAGIC_OPEN && NULL != openpath) {
            /* Special case, we've already got the path argument while
             * checking magic open() so we use it here.
             */
            strncpy(path, openpath, PATH_MAX);
        }
        else if (0 > trace_get_string(child->pid, arg, path, PATH_MAX)) {
            save_errno = errno;
            LOGE("Failed to get string from argument %d: %s", arg, strerror(errno));
            errno = save_errno;
            return RS_ERROR;
        }
    }
    else if (sdef->flags & CHECK_PATH2) {
        if (0 > trace_get_string(child->pid, arg, path, PATH_MAX)) {
            save_errno = errno;
            LOGE("Failed to get string from argument %d: %s", arg, strerror(errno));
            errno = save_errno;
            return RS_ERROR;
        }
    }
    else if (sdef->flags & CHECK_PATH_AT || sdef->flags & CHECK_PATH_AT2) {
        if (0 > syscall_process_pathat(child->pid, arg, path))
            return RS_ERROR;
    }

    if ('/' != path[0]) {
        // Add current working directory
        char *pathc;
        LOGD("`%s' is not an absolute path, adding cwd `%s'", path, child->cwd);
        pathc = xstrndup(path, PATH_MAX);
        snprintf(pathc, PATH_MAX, "%s/%s", child->cwd, path);
        strncpy(path, pathc, PATH_MAX);
        free(pathc);
    }

    if (syscall_can_creat(arg, sdef->flags) || (check_ret && RM_CREAT == ret)) {
        LOGC("System call may create the file, using wrapper function");
        if (sdef->flags & DONT_RESOLV)
            rpath = resolve_path(path, 0);
        else
            rpath = resolve_path(path, 1);
    }
    else {
        LOGC("System call can't create the file, using realpath()");
        if (sdef->flags & DONT_RESOLV)
            rpath = erealpath(path, NULL);
        else
            rpath = erealpath(path, NULL);
    }

    if (NULL == rpath) {
        child->retval = -errno;
        LOGD("realpath() failed for `%s': %s", path, strerror(errno));
        return RS_DENY;
    }

    ret = syscall_check_prefix(ctx, child, arg, sdef, path, rpath);
    free(rpath);
    return ret;
}

static enum res_syscall syscall_check_magic_open(context_t *ctx, struct tchild *child, const char *path) {
    int ismagic = 0, save_errno;
    const char *rpath;

    LOGD("Checking if open(\"%s\", ...) is magic", path);
    if (path_magic_write(path)) {
        ismagic = 1;
        rpath = path + CMD_WRITE_LEN - 1;
        LOGN("Approved addwrite(\"%s\") for child %i", rpath, child->pid);
        pathnode_new(&(ctx->write_prefixes), rpath);
    }
    else if (path_magic_predict(path)) {
        ismagic = 1;
        rpath = path + CMD_PREDICT_LEN - 1;
        LOGN("Approved addpredict(\"%s\") for child %i", rpath, child->pid);
        pathnode_new(&(ctx->predict_prefixes), rpath);
    }
    else if (path_magic_rmwrite(path)) {
        ismagic = 1;
        rpath = path + CMD_RMWRITE_LEN - 1;
        LOGN("Approved rmwrite(\"%s\") for child %i", rpath, child->pid);
        if (NULL != ctx->write_prefixes)
            pathnode_delete(&(ctx->write_prefixes), rpath);
    }
    else if (path_magic_rmpredict(path)) {
        ismagic = 1;
        rpath = path + CMD_RMPREDICT_LEN - 1;
        LOGN("Approved rmpredict(\"%s\") for child %i", rpath, child->pid);
        if (NULL != ctx->predict_prefixes)
            pathnode_delete(&(ctx->predict_prefixes), rpath);
    }

    if (ismagic) {
        // Change argument to /dev/null
        LOGD("Changing path to /dev/null");
        if (0 > trace_set_string(child->pid, 0, "/dev/null", 10)) {
            save_errno = errno;
            LOGE("Failed to set string to /dev/null: %s", strerror(errno));
            errno = save_errno;
            return RS_ERROR;
        }
        return RS_ALLOW;
    }

    LOGD("open(\"%s\", ...) not magic", path);
    return RS_NONMAGIC;
}

static enum res_syscall syscall_check_magic_stat(struct tchild *child) {
    int save_errno;
    char path[PATH_MAX];

    if (0 > trace_get_string(child->pid, 0, path, PATH_MAX)) {
        save_errno = errno;
        LOGE("Failed to get string from argument 0: %s", strerror(errno));
        errno = save_errno;
        return RS_ERROR;
    }
    LOGD("Checking if stat(\"%s\") is magic", path);
    if (path_magic_dir(path)) {
        LOGD("stat(\"%s\") is magic", path);
        if (0 > trace_set_string(child->pid, 0, "/dev/null", 10)) {
            save_errno = errno;
            LOGE("Failed to change path argument: %s", strerror(errno));
            errno = save_errno;
            return RS_ERROR;
        }
        return RS_ALLOW;
    }
    else {
        LOGD("stat(\"%s\") is not magic", path);
        return RS_NONMAGIC;
    }
}

enum res_syscall syscall_check(context_t *ctx, struct tchild *child, int syscall) {
    unsigned int i, ret, save_errno;
    char *openpath;
    const char *sname;
    const struct syscall_def *sdef;
    for (i = 0; syscalls[i].no != -1; i++) {
        if (syscalls[i].no == syscall)
            goto found;
    }
    return RS_ALLOW;
found:
    sdef = &(syscalls[i]);
    openpath = NULL;
    if (LOG_DEBUG <= log_level)
        sname = syscall_get_name(sdef->no);
    else
        sname = NULL;

    LOGD("Child %i called essential system call %s()", child->pid, sname);

    // Handle magic calls
    if (child->hasmagic) {
        if (sdef->flags & MAGIC_OPEN) {
            /* Special case, to avoid getting the path argument of open()
             * twice, one for this one and one for CHECK_PATH, we get it here and
             * pass it to syscall_check_path.
             */
            openpath = (char *) xmalloc(sizeof(char) * PATH_MAX);
            if (0 > trace_get_string(child->pid, 0, openpath, PATH_MAX)) {
                save_errno = errno;
                LOGE("Failed to get string from argument 0: %s", strerror(errno));
                errno = save_errno;
                return RS_ERROR;
            }
            ret = syscall_check_magic_open(ctx, child, openpath);
            if (RS_NONMAGIC != ret)
                return ret;
        }
        else if (sdef->flags & MAGIC_STAT) {
            ret = syscall_check_magic_stat(child);
            if (RS_NONMAGIC != ret)
                return ret;
        }
    }

    if (sdef->flags & CHECK_PATH) {
        LOGD("System call %s() has CHECK_PATH set, checking", sname);
        /* Return here only if access is denied because some syscalls have
         * both CHECK_PATH and CHECK_PATH2 set.
         */
        ret = syscall_check_path(ctx, child, 0, sdef, openpath);
        free(openpath);
        if (RS_ALLOW != ret)
            return ret;
    }
    if (sdef->flags & CHECK_PATH2) {
        LOGD("System call %s() has CHECK_PATH2 set, checking", sname);
        return syscall_check_path(ctx, child, 1, sdef, NULL);
    }
    if (sdef->flags & CHECK_PATH_AT) {
        LOGD("System call %s() has CHECK_PATH_AT set, checking", sname);
        ret = syscall_check_path(ctx, child, 1, sdef, NULL);
        if (RS_ALLOW != ret)
            return ret;
    }
    if (sdef->flags & CHECK_PATH_AT2) {
        LOGD("System call %s() has CHECK_PATH_AT2 set, checking", sname);
        return syscall_check_path(ctx, child, 3, sdef, NULL);
    }
    if (sdef->flags & NET_CALL && !(ctx->net_allowed)) {
#if defined(__NR_socketcall)
        access_error(child->pid, "socketcall()");
#elif defined(__NR_socket)
        access_error(child->pid, "socket()");
#endif
        child->retval = -EACCES;
        return RS_DENY;
    }
    return RS_ALLOW;
}

int syscall_handle(context_t *ctx, struct tchild *child) {
    int ret;
    long syscall;
    const char *sname;
    static int before_initial_execv = 1;

    if (0 > trace_get_syscall(child->pid, &syscall)) {
        if (ESRCH == errno)
            return handle_esrch(ctx, child);
        else
            DIESOFT("Failed to get syscall: %s", strerror(errno));
    }
    if (LOG_DEBUG <= log_level)
        sname = syscall_get_name(syscall);
    else
        sname = NULL;

    if (!(child->flags & TCHILD_INSYSCALL)) { // Entering syscall
        LOGC("Child %i is entering system call %s()", child->pid, sname);

        if (__NR_execve == syscall) {
            if(before_initial_execv)
                before_initial_execv = 0;
            else if (child->hasmagic) {
                LOGV("Child %i called execve() disallowing magic commands", child->pid);
                child->hasmagic = 0;
            }
        }

        ret = syscall_check(ctx, child, syscall);
        switch (ret) {
            case RS_DENY:
                LOGD("Denying access to system call %s()", sname);
                child->syscall = syscall;
                if (0 > trace_set_syscall(child->pid, 0xbadca11)) {
                    if (ESRCH == errno)
                        return handle_esrch(ctx, child);
                    else
                        DIESOFT("Failed to set syscall: %s", strerror(errno));
                }
                break;
            case RS_ALLOW:
                LOGC("Allowing access to system call %s()", sname);
                break;
            case RS_ERROR:
            default:
                if (ESRCH == errno)
                    return handle_esrch(ctx, child);
                else {
                    if (NULL == sname)
                        sname = syscall_get_name(syscall);
                    DIESOFT("Error while checking system call %s() for access: %s", sname,
                            strerror(errno));
                }
                break;
        }
        child->flags ^= TCHILD_INSYSCALL;
    }
    else { // Exiting syscall
        LOGC("Child %i is exiting system call %s()", child->pid, sname);
        if (0xbadca11 == syscall) {
            LOGD("Restoring real call number for denied system call %s()", sname);
            // Restore real call number and return our error code
            if (0 > trace_set_syscall(child->pid, child->syscall)) {
                if (ESRCH == errno)
                    return handle_esrch(ctx, child);
                else
                    DIESOFT("Failed to restore syscall: %s", strerror(errno));
            }
            if (0 > trace_set_return(child->pid, child->retval)) {
                if (ESRCH == errno)
                    return handle_esrch(ctx, child);
                else
                    DIESOFT("Failed to set return code: %s", strerror(errno));
            }
        }
        else if (__NR_chdir == syscall || __NR_fchdir == syscall) {
            long retval;
            if (0 > trace_get_return(child->pid, &retval)) {
                if (ESRCH == errno)
                    return handle_esrch(ctx, child);
                else
                    DIESOFT("Failed to get return code: %s", strerror(errno));
            }
            if (0 == retval) {
                // Child has successfully changed directory
                if (NULL == getcwd_pid(child->cwd, PATH_MAX, child->pid))
                    DIESOFT("Failed to get current working directory of child %i: %s",
                            child->pid, strerror(errno));
                LOGV("Child %i has changed directory to '%s'", child->pid, child->cwd);
            }
        }
        child->flags ^= TCHILD_INSYSCALL;
    }
    return 0;
}
