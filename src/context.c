/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Ali Polatel
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

#include <errno.h>
#include <string.h>
#include <sysexits.h>

#include <glib.h>

#include "defs.h"
#include "getcwd.h"
#include "util.h"
#include "context.h"
#include "children.h"

context_t *context_new(void) {
    context_t *ctx;
    ctx = (context_t *) g_malloc (sizeof(context_t));
    ctx->paranoid = 0;
    ctx->cwd = egetcwd();
    if (NULL == ctx->cwd)
        DIESOFT("Failed to get current working directory: %s", strerror(errno));
    ctx->children = NULL;
    ctx->eldest = NULL;
    return ctx;
}

void context_free(context_t *ctx) {
    if (NULL != ctx->cwd)
        g_free (ctx->cwd);
    if (NULL != ctx->children)
        tchild_free(&(ctx->children));
    g_free (ctx);
}
