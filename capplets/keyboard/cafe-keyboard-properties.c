/* -*- mode: c; style: linux -*- */

/* keyboard-properties.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2001 Jonathan Blandford
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Rachel Hestilow <hestilow@ximian.com>
 *	       Jonathan Blandford <jrb@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <gio/gio.h>

#include "capplet-util.h"
#include "activate-settings-daemon.h"

#include "cafe-keyboard-properties-a11y.h"
#include "cafe-keyboard-properties-xkb.h"

#define KEYBOARD_SCHEMA "org.cafe.peripherals-keyboard"
#define INTERFACE_SCHEMA "org.cafe.interface"
#define TYPING_BREAK_SCHEMA "org.cafe.typing-break"

enum {
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static GSettings * keyboard_settings = NULL;
static GSettings * interface_settings = NULL;
static GSettings * typing_break_settings = NULL;

static CtkBuilder *
create_dialog (void)
{
	CtkBuilder *dialog;
	CtkSizeGroup *size_group;
	CtkWidget *image;
	GError *error = NULL;

	dialog = ctk_builder_new ();
	if (ctk_builder_add_from_resource (dialog, "/org/cafe/ccc/keyboard/cafe-keyboard-properties-dialog.ui", &error) == 0) {
		g_warning ("Could not load UI: %s", error->message);
		g_error_free (error);
		g_object_unref (dialog);
		return NULL;
	}

	size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
	ctk_size_group_add_widget (size_group, WID ("repeat_slow_label"));
	ctk_size_group_add_widget (size_group, WID ("delay_short_label"));
	ctk_size_group_add_widget (size_group, WID ("blink_slow_label"));
	g_object_unref (G_OBJECT (size_group));

	size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
	ctk_size_group_add_widget (size_group, WID ("repeat_fast_label"));
	ctk_size_group_add_widget (size_group, WID ("delay_long_label"));
	ctk_size_group_add_widget (size_group, WID ("blink_fast_label"));
	g_object_unref (G_OBJECT (size_group));

	size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
	ctk_size_group_add_widget (size_group, WID ("repeat_delay_scale"));
	ctk_size_group_add_widget (size_group, WID ("repeat_speed_scale"));
	ctk_size_group_add_widget (size_group, WID ("cursor_blink_time_scale"));
	g_object_unref (G_OBJECT (size_group));

	image = ctk_image_new_from_icon_name ("list-add", CTK_ICON_SIZE_BUTTON);
	ctk_button_set_image (CTK_BUTTON (WID ("xkb_layouts_add")), image);

	image = ctk_image_new_from_icon_name ("view-refresh", CTK_ICON_SIZE_BUTTON);
	ctk_button_set_image (CTK_BUTTON (WID ("xkb_reset_to_defaults")), image);

	return dialog;
}

static void
dialog_response (CtkWidget * widget,
		 gint response_id, guint data)
{
	if (response_id == CTK_RESPONSE_HELP)
		capplet_help (CTK_WINDOW (widget), "goscustperiph-2");
	else
		ctk_main_quit ();
}

static void
setup_dialog (CtkBuilder * dialog)
{
	gchar *monitor;

	g_settings_bind (keyboard_settings,
					 "repeat",
					 WID ("repeat_toggle"),
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (keyboard_settings,
					 "repeat",
					 WID ("repeat_table"),
					 "sensitive",
					 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (keyboard_settings,
					 "delay",
					 ctk_range_get_adjustment (CTK_RANGE (WID ("repeat_delay_scale"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (keyboard_settings,
					 "rate",
					 ctk_range_get_adjustment (CTK_RANGE (WID ("repeat_speed_scale"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (interface_settings,
					 "cursor-blink",
					 WID ("cursor_toggle"),
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (interface_settings,
					 "cursor-blink",
					 WID ("cursor_hbox"),
					 "sensitive",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (interface_settings,
					 "cursor-blink-time",
					 ctk_range_get_adjustment (CTK_RANGE (WID ("cursor_blink_time_scale"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);

	/* Ergonomics */
	monitor = g_find_program_in_path ("cafe-typing-monitor");
	if (monitor != NULL) {
		g_free (monitor);

		g_settings_bind (typing_break_settings,
						 "enabled",
						 WID ("break_enabled_toggle"),
						 "active",
						 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (typing_break_settings,
						 "enabled",
						 WID ("break_details_table"),
						 "sensitive",
						 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (typing_break_settings,
						 "type-time",
						 WID ("break_enabled_spin"),
						 "value",
						 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (typing_break_settings,
						 "break-time",
						 WID ("break_interval_spin"),
						 "value",
						 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (typing_break_settings,
						 "allow-postpone",
						 WID ("break_postponement_toggle"),
						 "active",
						 G_SETTINGS_BIND_DEFAULT);

	} else {
		/* don't show the typing break tab if the daemon is not available */
		CtkNotebook *nb = CTK_NOTEBOOK (WID ("keyboard_notebook"));
		gint tb_page = ctk_notebook_page_num (nb, WID ("break_enabled_toggle"));
		ctk_notebook_remove_page (nb, tb_page);
	}

	g_signal_connect (WID ("keyboard_dialog"), "response",
			  (GCallback) dialog_response, NULL);

	setup_xkb_tabs (dialog);
	setup_a11y_tabs (dialog);
}

int
main (int argc, char **argv)
{
	CtkBuilder *dialog;
	GOptionContext *context;

	static gboolean apply_only = FALSE;
	static gboolean switch_to_typing_break_page = FALSE;
	static gboolean switch_to_a11y_page = FALSE;

	static GOptionEntry cap_options[] = {
		{"apply", 0, 0, G_OPTION_ARG_NONE, &apply_only,
		 N_
		 ("Just apply settings and quit (compatibility only; now handled by daemon)"),
		 NULL},
		{"init-session-settings", 0, 0, G_OPTION_ARG_NONE,
		 &apply_only,
		 N_
		 ("Just apply settings and quit (compatibility only; now handled by daemon)"),
		 NULL},
		{"typing-break", 0, 0, G_OPTION_ARG_NONE,
		 &switch_to_typing_break_page,
		 N_
		 ("Start the page with the typing break settings showing"),
		 NULL},
		{"a11y", 0, 0, G_OPTION_ARG_NONE,
		 &switch_to_a11y_page,
		 N_
		 ("Start the page with the accessibility settings showing"),
		 NULL},
		{NULL}
	};


	context = g_option_context_new (_("- CAFE Keyboard Preferences"));
	g_option_context_add_main_entries (context, cap_options,
					   GETTEXT_PACKAGE);

	capplet_init (context, &argc, &argv);

	activate_settings_daemon ();

	keyboard_settings = g_settings_new (KEYBOARD_SCHEMA);
	interface_settings = g_settings_new (INTERFACE_SCHEMA);
	typing_break_settings = g_settings_new (TYPING_BREAK_SCHEMA);

	dialog = create_dialog ();
	if (!dialog) /* Warning was already printed to console */
		exit (EXIT_FAILURE);

	setup_dialog (dialog);

        CtkNotebook* nb = CTK_NOTEBOOK (WID ("keyboard_notebook"));
        ctk_widget_add_events (CTK_WIDGET (nb), CDK_SCROLL_MASK);
        g_signal_connect (CTK_WIDGET (nb), "scroll-event",
                          G_CALLBACK (capplet_dialog_page_scroll_event_cb),
                          CTK_WINDOW (WID ("keyboard_dialog")));

	if (switch_to_typing_break_page) {
		ctk_notebook_set_current_page (nb, 4);
	}
	else if (switch_to_a11y_page) {
		ctk_notebook_set_current_page (nb, 2);

	}

	capplet_set_icon (WID ("keyboard_dialog"),
			  "input-keyboard");
	ctk_widget_show (WID ("keyboard_dialog"));

	g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);

	ctk_main ();

	finalize_a11y_tabs ();
	g_object_unref (keyboard_settings);
	g_object_unref (interface_settings);
	g_object_unref (typing_break_settings);

	return 0;
}
