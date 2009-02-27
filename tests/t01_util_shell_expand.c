/* Check program for t01-util-bash-expand.bash
 * vim: set et ts=4 sts=4 sw=4 fdm=syntax :
 * Copyright 2009 Ali Polatel <polatel@gmail.com>
 * Distributed under the terms of the GNU General Public License v2
 */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "../src/defs.h"

int main(int argc, char **argv) {
    if (1 == argc)
        return EXIT_FAILURE;

    char dest[PATH_MAX];
    shell_expand(dest, argv[1]);

    printf("%s", dest);
    return EXIT_SUCCESS;
}
