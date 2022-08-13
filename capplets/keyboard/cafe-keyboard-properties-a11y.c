/* -*- mode: c; style: linux -*- */

/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Denis Washington <denisw@svn.gnome.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "cafe-keyboard-properties-a11y.h"
#include <gio/gio.h>
#include "capplet-util.h"

#define NWID(s) GTK_WIDGET (ctk_builder_get_object (notifications_dialog, s))

#define A11Y_SCHEMA "org.cafe.accessibility-keyboard"
#define CROMA_SCHEMA "org.cafe.Croma.general"

static CtkBuilder *notifications_dialog = NULL;
static GSettings *a11y_settings = NULL;

enum
{
	VISUAL_BELL_TYPE_INVALID,
	VISUAL_BELL_TYPE_FULLSCREEN,
	VISUAL_BELL_TYPE_FRAME_FLASH
};

static void
stickykeys_enable_toggled_cb (CtkWidget *w, CtkBuilder *dialog)
{
	gboolean active = ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

	ctk_widget_set_sensitive (WID ("stickykeys_latch_to_lock"), active);
	ctk_widget_set_sensitive (WID ("stickykeys_two_key_off"), active);
	if (notifications_dialog)
		ctk_widget_set_sensitive (NWID ("stickykeys_notifications_box"), active);
}

static void
slowkeys_enable_toggled_cb (CtkWidget *w, CtkBuilder *dialog)
{
	gboolean active = ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

	ctk_widget_set_sensitive (WID ("slowkeys_delay_box"), active);
	if (notifications_dialog)
		ctk_widget_set_sensitive (NWID ("slowkeys_notifications_box"), active);
}

static void
bouncekeys_enable_toggled_cb (CtkWidget *w, CtkBuilder *dialog)
{
	gboolean active = ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

	ctk_widget_set_sensitive (WID ("bouncekeys_delay_box"), active);
	if (notifications_dialog)
		ctk_widget_set_sensitive (NWID ("bouncekeys_notifications_box"), active);
}

static void
visual_bell_enable_toggled_cb (CtkWidget *w, CtkBuilder *dialog)
{
	gboolean active = ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

	if (notifications_dialog) {
		ctk_widget_set_sensitive (NWID ("visual_bell_titlebar"), active);
		ctk_widget_set_sensitive (NWID ("visual_bell_fullscreen"), active);
	}
}

static void
bell_flash_gsettings_changed (GSettings *settings, gchar *key, CtkBuilder *dialog)
{
	int bell_flash_type = g_settings_get_enum (settings, key);
	if (bell_flash_type == VISUAL_BELL_TYPE_FULLSCREEN)
	{
		ctk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NWID ("visual_bell_fullscreen")), TRUE);
	}
	else if (bell_flash_type == VISUAL_BELL_TYPE_FRAME_FLASH)
	{
		ctk_toggle_button_set_active (GTK_TOGGLE_BUTTON (NWID ("visual_bell_titlebar")), TRUE);
	}
}

static void
bell_flash_radio_changed (CtkWidget *widget, CtkBuilder *builder)
{
	GSettings *croma_settings;
	croma_settings = g_settings_new (CROMA_SCHEMA);
	int old_bell_flash_type = g_settings_get_enum (croma_settings, "visual-bell-type");
	int new_bell_flash_type = VISUAL_BELL_TYPE_INVALID;

	if (ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (NWID ("visual_bell_fullscreen"))))
		new_bell_flash_type = VISUAL_BELL_TYPE_FULLSCREEN;
	else if (ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (NWID ("visual_bell_titlebar"))))
		new_bell_flash_type = VISUAL_BELL_TYPE_FRAME_FLASH;

	if (old_bell_flash_type != new_bell_flash_type)
		g_settings_set_enum (croma_settings, "visual-bell-type", new_bell_flash_type);

	g_object_unref (croma_settings);
}

static void
a11y_notifications_dialog_response_cb (CtkWidget *w, gint response)
{
	if (response == GTK_RESPONSE_HELP) {
		capplet_help (GTK_WINDOW (w),
		              "prefs-keyboard#goscustdesk-TBL-86");
	}
	else {
		ctk_widget_destroy (w);
	}
}
static void
notifications_button_clicked_cb (CtkWidget *button, CtkBuilder *dialog)
{
	CtkWidget *w;

	notifications_dialog = ctk_builder_new ();
	ctk_builder_add_from_resource (notifications_dialog,
	                               "/org/cafe/mcc/keyboard/cafe-keyboard-properties-a11y-notifications.ui",
	                               NULL);

	stickykeys_enable_toggled_cb (WID ("stickykeys_enable"), dialog);
	slowkeys_enable_toggled_cb (WID ("slowkeys_enable"), dialog);
	bouncekeys_enable_toggled_cb (WID ("bouncekeys_enable"), dialog);

	w = NWID ("feature_state_change_beep");
	g_settings_bind (a11y_settings, "feature-state-change-beep", w, "active", G_SETTINGS_BIND_DEFAULT);

	w = NWID ("togglekeys_enable");
	g_settings_bind (a11y_settings, "togglekeys-enable", w, "active", G_SETTINGS_BIND_DEFAULT);

	w = NWID ("stickykeys_modifier_beep");
	g_settings_bind (a11y_settings, "stickykeys-modifier-beep", w, "active", G_SETTINGS_BIND_DEFAULT);

	w = NWID ("slowkeys_beep_press");
	g_settings_bind (a11y_settings, "slowkeys-beep-press", w, "active", G_SETTINGS_BIND_DEFAULT);

	w = NWID ("slowkeys_beep_accept");
	g_settings_bind (a11y_settings, "slowkeys-beep-accept", w, "active", G_SETTINGS_BIND_DEFAULT);

	w = NWID ("slowkeys_beep_reject");
	g_settings_bind (a11y_settings, "slowkeys-beep-reject", w, "active", G_SETTINGS_BIND_DEFAULT);

	w = NWID ("bouncekeys_beep_reject");
	g_settings_bind (a11y_settings, "bouncekeys-beep-reject", w, "active", G_SETTINGS_BIND_DEFAULT);

	GSettings *croma_settings = g_settings_new (CROMA_SCHEMA);
	w = NWID ("visual_bell_enable");
	g_settings_bind (croma_settings, "visual-bell", w, "active", G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (w, "toggled",
			G_CALLBACK (visual_bell_enable_toggled_cb), dialog);
	visual_bell_enable_toggled_cb (w, dialog);

	bell_flash_gsettings_changed (croma_settings, "visual-bell-type", NULL);
	g_signal_connect (NWID ("visual_bell_titlebar"), "clicked",
					  G_CALLBACK(bell_flash_radio_changed),
					  notifications_dialog);
	g_signal_connect (NWID ("visual_bell_fullscreen"), "clicked",
					  G_CALLBACK(bell_flash_radio_changed),
					  notifications_dialog);
	g_signal_connect (croma_settings,
					  "changed::visual-bell-type",
					  G_CALLBACK (bell_flash_gsettings_changed),
					  notifications_dialog);

	w = NWID ("a11y_notifications_dialog");
	ctk_window_set_transient_for (GTK_WINDOW (w),
	                              GTK_WINDOW (WID ("keyboard_dialog")));
	g_signal_connect (w, "response",
			  G_CALLBACK (a11y_notifications_dialog_response_cb), NULL);

	ctk_dialog_run (GTK_DIALOG (w));

	g_object_unref (croma_settings);
	g_object_unref (notifications_dialog);
	notifications_dialog = NULL;
}

static void
mousekeys_enable_toggled_cb (CtkWidget *w, CtkBuilder *dialog)
{
	gboolean active = ctk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
	ctk_widget_set_sensitive (WID ("mousekeys_table"), active);
}

void
finalize_a11y_tabs (void)
{
	g_object_unref (a11y_settings);
}

void
setup_a11y_tabs (CtkBuilder *dialog)
{
	CtkWidget *w;

	a11y_settings = g_settings_new (A11Y_SCHEMA);

	/* Accessibility tab */
	g_settings_bind (a11y_settings,
					 "enable",
					 WID ("master_enable"),
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	w = WID ("stickykeys_enable");
	g_settings_bind (a11y_settings,
					 "stickykeys-enable",
					 w,
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (w, "toggled",
			  G_CALLBACK (stickykeys_enable_toggled_cb), dialog);
	stickykeys_enable_toggled_cb (w, dialog);

	g_settings_bind (a11y_settings,
					 "stickykeys-latch-to-lock",
					 WID ("stickykeys_latch_to_lock"),
					 "active",
					 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (a11y_settings,
					 "stickykeys-two-key-off",
					 WID ("stickykeys_two_key_off"),
					 "active",
					 G_SETTINGS_BIND_DEFAULT);

	w = WID ("slowkeys_enable");
	g_settings_bind (a11y_settings,
					 "slowkeys-enable",
					 w,
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (w, "toggled",
			  G_CALLBACK (slowkeys_enable_toggled_cb), dialog);
	slowkeys_enable_toggled_cb (w, dialog);

	w = WID ("bouncekeys_enable");
	g_settings_bind (a11y_settings,
					 "bouncekeys-enable",
					 w,
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (w, "toggled",
			  G_CALLBACK (bouncekeys_enable_toggled_cb), dialog);
	bouncekeys_enable_toggled_cb (w, dialog);

	g_settings_bind (a11y_settings,
					 "slowkeys-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("slowkeys_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (a11y_settings,
					 "bouncekeys-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("bouncekeys_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);

	w = WID ("notifications_button");
	g_signal_connect (w, "clicked",
			  G_CALLBACK (notifications_button_clicked_cb), dialog);

	/* Mouse Keys tab */

	w = WID ("mousekeys_enable");
	g_settings_bind (a11y_settings,
					 "mousekeys-enable",
					 w,
					 "active",
					 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (w, "toggled",
			  G_CALLBACK (mousekeys_enable_toggled_cb), dialog);
	mousekeys_enable_toggled_cb (w, dialog);

	g_settings_bind (a11y_settings,
					 "slowkeys-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("slowkeys_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (a11y_settings,
					 "bouncekeys-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("bouncekeys_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (a11y_settings,
					 "slowkeys-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("slowkeys_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (a11y_settings,
					 "bouncekeys-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("bouncekeys_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (a11y_settings,
					 "mousekeys-accel-time",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("mousekeys_accel_time_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (a11y_settings,
					 "mousekeys-max-speed",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("mousekeys_max_speed_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (a11y_settings,
					 "mousekeys-init-delay",
					 ctk_range_get_adjustment (GTK_RANGE (WID ("mousekeys_init_delay_slide"))),
					 "value",
					 G_SETTINGS_BIND_DEFAULT);
}
