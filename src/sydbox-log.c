/* vim: set sw=4 sts=4 et foldmethod=syntax : */

/*
 * Copyright (c) 2009 Saleem Abdulrasool <compnerd@compnerd.org>
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

#include "sydbox-log.h"
#include "sydbox-config.h"

#include <glib/gstdio.h>

#include <errno.h>
#include <unistd.h>

static FILE *fd;
static gboolean initialized;

static inline void
sydbox_log_output (const gchar *log_domain,
                   GLogLevelFlags log_level,
                   const gchar *message)
{
    gchar *prefix, *output;

    g_return_if_fail (initialized);
    g_return_if_fail (message != NULL && message[0] != '\0');

    switch (log_level)
    {
        case G_LOG_LEVEL_CRITICAL:
            prefix = g_strdup ("CRITICAL");
            break;
        case G_LOG_LEVEL_WARNING:
            prefix = g_strdup ("WARNING");
            break;
        case G_LOG_LEVEL_MESSAGE:
            prefix = g_strdup ("Message");
            break;
        case G_LOG_LEVEL_INFO:
            prefix = g_strdup ("INFO");
            break;
        case G_LOG_LEVEL_DEBUG:
            prefix = g_strdup_printf ("(%s:%lu): DEBUG", g_get_prgname(), (gulong) getpid());
            break;
        default:
            prefix = g_strdup ("");
            break;
    }


    output = g_strdup_printf ("%s %s: %s\n",
                              log_domain ? log_domain : "**", prefix, message);
    g_free (prefix);

    g_fprintf (fd ? fd : stderr, "%s", output);
    fflush (fd ? fd : stderr);

    g_free (output);
}

static void
sydbox_log_handler (const gchar *log_domain,
                    GLogLevelFlags log_level,
                    const gchar *message,
                    gpointer user_data G_GNUC_UNUSED)
{
    if ( ((log_level & G_LOG_LEVEL_MESSAGE)   && sydbox_config_get_verbosity () < 1) ||
         ((log_level & G_LOG_LEVEL_INFO)      && sydbox_config_get_verbosity () < 2) ||
         ((log_level & G_LOG_LEVEL_DEBUG)     && sydbox_config_get_verbosity () < 3) ||
         ((log_level & LOG_LEVEL_DEBUG_TRACE) && sydbox_config_get_verbosity () < 4) )
        return;

    sydbox_log_output (log_domain, log_level, message);
}

void
sydbox_log_init (void)
{
    if (initialized)
        return;

    if (sydbox_config_get_log_file ()) {
        fd = g_fopen (sydbox_config_get_log_file (), "a");
        if (! fd) {
            const gchar *error_string = g_strerror (errno);
            g_printerr ("could not open log '%s': %s\n", sydbox_config_get_log_file (), error_string);
            g_printerr ("all logging will go to stderr\n");
        }
    }

    g_log_set_default_handler (sydbox_log_handler, NULL);

    initialized = TRUE;
}

void
sydbox_log_fini (void)
{
    if (! initialized)
        return;

    if (fd)
        fclose (fd);

    initialized = FALSE;
}
