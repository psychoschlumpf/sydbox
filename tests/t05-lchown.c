/* Check program for t05-lchown.bash
 * vim: set et ts=4 sts=4 sw=4 fdm=syntax :
 * Copyright 2009 Ali Polatel <polatel@gmail.com>
 * Distributed under the terms of the GNU General Public License v2
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(void) {
    if (0 > lchown("arnold.layne", 0, 0)) {
        if (EPERM == errno) {
            /* It's the operating system not sydbox who denied access. */
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }
    else
        return EXIT_SUCCESS;
}
