/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <cdk/cdkx.h>
#include <ctk/ctk.h>

#include "drw-selection.h"
#include "drwright.h"

gboolean debug = FALSE;

#ifndef HAVE_APP_INDICATOR
static gboolean
have_tray (void)
{
	Screen *xscreen = DefaultScreenOfDisplay (cdk_x11_display_get_xdisplay(cdk_display_get_default()));
	Atom    selection_atom;
	char   *selection_atom_name;

	selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					       XScreenNumberOfScreen (xscreen));
	selection_atom = cdk_x11_get_xatom_by_name (selection_atom_name);
	g_free (selection_atom_name);

	if (XGetSelectionOwner (DisplayOfScreen (xscreen), selection_atom)) {
		return TRUE;
	} else {
		return FALSE;
	}
}
#endif /* HAVE_APP_INDICATOR */

int
main (int argc, char *argv[])
{
	DrwSelection *selection;
	gboolean      no_check = FALSE;
        const GOptionEntry options[] = {
          { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug,
            N_("Enable debugging code"), NULL },
          { "no-check", 'n', 0, G_OPTION_ARG_NONE, &no_check,
            N_("Don't check whether the notification area exists"), NULL },
	  { NULL }
        };
        GOptionContext *option_context;
        GError *error = NULL;
        gboolean retval;

	bindtextdomain (GETTEXT_PACKAGE, CAFELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

        option_context = g_option_context_new (NULL);
        g_option_context_set_translation_domain (option_context, GETTEXT_PACKAGE);
        g_option_context_add_main_entries (option_context, options, GETTEXT_PACKAGE);
        g_option_context_add_group (option_context, ctk_get_option_group (TRUE));

        retval = g_option_context_parse (option_context, &argc, &argv, &error);
        g_option_context_free (option_context);
        if (!retval) {
                g_print ("%s\n", error->message);
                g_error_free (error);
                exit (1);
        }

	g_set_application_name (_("Typing Monitor"));
	ctk_window_set_default_icon_name ("cafe-typing-monitor");

	selection = drw_selection_start ();
	if (!drw_selection_is_master (selection)) {
		g_message ("The typing monitor is already running, exiting.");
		return 0;
	}

#ifndef HAVE_APP_INDICATOR
	if (!no_check && !have_tray ()) {
		CtkWidget *dialog;

		dialog = ctk_message_dialog_new (
			NULL, 0,
			CTK_MESSAGE_INFO,
			CTK_BUTTONS_CLOSE,
			_("The typing monitor uses the notification area to display "
			  "information. You don't seem to have a notification area "
			  "on your panel. You can add it by right-clicking on your "
			  "panel and choosing 'Add to panel', selecting 'Notification "
			  "area' and clicking 'Add'."));

		ctk_dialog_run (CTK_DIALOG (dialog));

		ctk_widget_destroy (dialog);
	}
#endif /* HAVE_APP_INDICATOR */

	drwright_new ();

	g_object_set (ctk_settings_get_default (), "ctk-menu-images", TRUE, NULL);

	ctk_main ();

	return 0;
}
