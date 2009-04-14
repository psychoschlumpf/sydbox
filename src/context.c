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

#include <errno.h>
#include <stdlib.h>

#include <glib.h>

#include "context.h"
#include "wrappers.h"
#include "sydbox-log.h"

context_t *
context_new (void)
{
    context_t *ctx;

    ctx = (context_t *) g_malloc0 (sizeof (context_t));

    if (! (ctx->cwd = egetcwd())) {
        g_printerr ("failed to get current working directory: %s", g_strerror (errno));
        exit (-1);
    }

    return ctx;
}

void
context_free (context_t *ctx)
{
    if (NULL != ctx->cwd)
        g_free (ctx->cwd);

    if (NULL != ctx->children)
        tchild_free (&(ctx->children));

    g_free (ctx);
}

int
context_remove_child (context_t * const ctx, const struct tchild * const child)
{
    g_info ("removing child %d from context", child->pid);

    if (ctx->eldest == child->pid)
        return -1;

    tchild_delete (&ctx->children, child->pid);

    return 0;
}

