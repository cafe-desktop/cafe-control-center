/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 CodeFactory AB
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
#include <math.h>
#include <glib/gi18n.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <cdk/cdkkeysyms.h>
#include <ctk/ctk.h>
#include <gio/gio.h>

#ifdef HAVE_APP_INDICATOR
#include <libappindicator/app-indicator.h>
#endif /* HAVE_APP_INDICATOR */

#define CAFE_DESKTOP_USE_UNSTABLE_API
#include <libcafe-desktop/cafe-desktop-utils.h>

#include "drwright.h"
#include "drw-break-window.h"
#include "drw-monitor.h"
#include "drw-utils.h"
#include "drw-timer.h"

#ifndef HAVE_APP_INDICATOR
#define BLINK_TIMEOUT        200
#define BLINK_TIMEOUT_MIN    120
#define BLINK_TIMEOUT_FACTOR 100
#endif /* HAVE_APP_INDICATOR */

#define POPUP_ITEM_ENABLED 1
#define POPUP_ITEM_BREAK   2

typedef enum {
	STATE_START,
	STATE_RUNNING,
	STATE_WARN,
	STATE_BREAK_SETUP,
	STATE_BREAK,
	STATE_BREAK_DONE_SETUP,
	STATE_BREAK_DONE
} DrwState;

#ifdef HAVE_APP_INDICATOR
#define TYPING_MONITOR_ACTIVE_ICON "bar-green"
#define TYPING_MONITOR_ATTENTION_ICON "bar-red"
#endif /* HAVE_APP_INDICATOR */

struct _DrWright {
	/* Widgets. */
	CtkWidget       *break_window;
	GList           *secondary_break_windows;

	DrwMonitor      *monitor;

	CtkUIManager    *ui_manager;

	DrwState         state;
	DrwTimer        *timer;
	DrwTimer        *idle_timer;

	gint             last_elapsed_time;
	gint             save_last_time;

	/* Time settings. */
	gint             type_time;
	gint             break_time;
	gint             warn_time;

	gboolean         enabled;

	guint            clock_timeout_id;
#ifdef HAVE_APP_INDICATOR
	AppIndicator    *indicator;
#else
	guint            blink_timeout_id;

	gboolean         blink_on;

	CtkStatusIcon   *icon;

	cairo_surface_t *neutral_bar;
	cairo_surface_t *red_bar;
	cairo_surface_t *green_bar;
	cairo_surface_t *disabled_bar;
	CdkPixbuf       *composite_bar;
#endif /* HAVE_APP_INDICATOR */

	CtkWidget      *warn_dialog;
};

static void     activity_detected_cb           (DrwMonitor     *monitor,
						DrWright       *drwright);
static gboolean maybe_change_state             (DrWright       *drwright);
static gint     get_time_left                  (DrWright       *drwright);
static gboolean update_status                  (DrWright       *drwright);
static void     break_window_done_cb           (CtkWidget      *window,
						DrWright       *dr);
static void     break_window_postpone_cb       (CtkWidget      *window,
						DrWright       *dr);
static void     break_window_destroy_cb        (CtkWidget      *window,
						DrWright       *dr);
static void     popup_break_cb                 (CtkAction      *action,
						DrWright       *dr);
static void     popup_preferences_cb           (CtkAction      *action,
						DrWright       *dr);
static void     popup_about_cb                 (CtkAction      *action,
						DrWright       *dr);
#ifdef HAVE_APP_INDICATOR
static void     init_app_indicator             (DrWright       *dr);
#else
static void     init_tray_icon                 (DrWright       *dr);
#endif /* HAVE_APP_INDICATOR */
static GList *  create_secondary_break_windows (void);

static const CtkActionEntry actions[] = {
  {"Preferences", "preferences-desktop", N_("_Preferences"), NULL, NULL, G_CALLBACK (popup_preferences_cb)},
  {"About", "help-about", N_("_About"), NULL, NULL, G_CALLBACK (popup_about_cb)},
  {"TakeABreak", NULL, N_("_Take a Break"), NULL, NULL, G_CALLBACK (popup_break_cb)}
};

extern gboolean debug;

static void
setup_debug_values (DrWright *dr)
{
	dr->type_time = 5;
	dr->warn_time = 4;
	dr->break_time = 10;
}

#ifdef HAVE_APP_INDICATOR
static void
update_app_indicator (DrWright *dr)
{
	AppIndicatorStatus new_status;

	if (!dr->enabled) {
		app_indicator_set_status (dr->indicator,
					  APP_INDICATOR_STATUS_PASSIVE);
		return;
	}

	switch (dr->state) {
	case STATE_WARN:
	case STATE_BREAK_SETUP:
	case STATE_BREAK:
		new_status = APP_INDICATOR_STATUS_ATTENTION;
		break;
	default:
		new_status = APP_INDICATOR_STATUS_ACTIVE;
	}

	app_indicator_set_status (dr->indicator, new_status);
}
#else

static void
set_status_icon (CtkStatusIcon *icon, cairo_surface_t *surface)
{
	CdkPixbuf *pixbuf;

	pixbuf = cdk_pixbuf_get_from_surface (surface, 0, 0,
					      cairo_image_surface_get_width (surface),
					      cairo_image_surface_get_height (surface));

	ctk_status_icon_set_from_pixbuf (icon, pixbuf);

	g_object_unref (pixbuf);
}

static void
update_icon (DrWright *dr)
{
	CdkPixbuf *pixbuf;
	CdkPixbuf *tmp_pixbuf;
	gint       width, height;
	gfloat     r;
	gint       offset;
	gboolean   set_pixbuf;

	if (!dr->enabled) {
		set_status_icon (dr->icon, dr->disabled_bar);
		return;
	}

	width = cairo_image_surface_get_width (dr->neutral_bar);
	height = cairo_image_surface_get_height (dr->neutral_bar);

	tmp_pixbuf = cdk_pixbuf_get_from_surface (dr->neutral_bar,
						  0, 0,
						  width, height);

	set_pixbuf = TRUE;

	switch (dr->state) {
	case STATE_BREAK:
	case STATE_BREAK_SETUP:
		r = 1;
		break;

	case STATE_BREAK_DONE:
	case STATE_BREAK_DONE_SETUP:
	case STATE_START:
		r = 0;
		break;

	default:
		r = (float) (drw_timer_elapsed (dr->timer) + dr->save_last_time) /
		    (float) dr->type_time;
		break;
	}

	offset = CLAMP ((height - 0) * (1.0 - r), 1, height - 0);

	switch (dr->state) {
	case STATE_WARN:
		pixbuf = cdk_pixbuf_get_from_surface (dr->red_bar, 0, 0, width, height);
		set_pixbuf = FALSE;
		break;

	case STATE_BREAK_SETUP:
	case STATE_BREAK:
		pixbuf = cdk_pixbuf_get_from_surface (dr->red_bar, 0, 0, width, height);
		break;

	default:
		pixbuf = cdk_pixbuf_get_from_surface (dr->green_bar, 0, 0, width, height);
	}

	cdk_pixbuf_composite (pixbuf,
			      tmp_pixbuf,
			      0,
			      offset,
			      width,
			      height - offset,
			      0,
			      0,
			      1.0,
			      1.0,
			      CDK_INTERP_BILINEAR,
			      255);

	if (set_pixbuf) {
		ctk_status_icon_set_from_pixbuf (dr->icon,
						 tmp_pixbuf);
	}

	if (dr->composite_bar) {
		g_object_unref (dr->composite_bar);
	}

	dr->composite_bar = tmp_pixbuf;

	g_object_unref (pixbuf);
}

static gboolean
blink_timeout_cb (DrWright *dr)
{
	gfloat r;
	gint   timeout;

	r = (dr->type_time - drw_timer_elapsed (dr->timer) - dr->save_last_time) / dr->warn_time;
	timeout = BLINK_TIMEOUT + BLINK_TIMEOUT_FACTOR * r;

	if (timeout < BLINK_TIMEOUT_MIN) {
		timeout = BLINK_TIMEOUT_MIN;
	}

	if (dr->blink_on || timeout == 0) {
		ctk_status_icon_set_from_pixbuf (dr->icon, dr->composite_bar);
	} else {
		set_status_icon (dr->icon, dr->neutral_bar);
	}

	dr->blink_on = !dr->blink_on;

	if (timeout) {
		dr->blink_timeout_id = g_timeout_add (timeout,
						      (GSourceFunc) blink_timeout_cb,
						      dr);
	} else {
		dr->blink_timeout_id = 0;
	}

	return FALSE;
}
#endif /* HAVE_APP_INDICATOR */

static void
start_blinking (DrWright *dr)
{
#ifndef HAVE_APP_INDICATOR
	if (!dr->blink_timeout_id) {
		dr->blink_on = TRUE;
		blink_timeout_cb (dr);
	}

	/*ctk_widget_show (CTK_WIDGET (dr->icon));*/
#endif /* HAVE_APP_INDICATOR */
}

static void
stop_blinking (DrWright *dr)
{
#ifndef HAVE_APP_INDICATOR
	if (dr->blink_timeout_id) {
		g_source_remove (dr->blink_timeout_id);
		dr->blink_timeout_id = 0;
	}

	/*ctk_widget_hide (CTK_WIDGET (dr->icon));*/
#endif /* HAVE_APP_INDICATOR */
}

static gboolean
grab_keyboard_on_window (CdkWindow *window,
			 guint32    activate_time)
{
	CdkDisplay *display;
	CdkSeat *seat;
	CdkGrabStatus status;

	display = cdk_window_get_display (window);
	seat = cdk_display_get_default_seat (display);

	status = cdk_seat_grab (seat,
	                        window,
	                        CDK_SEAT_CAPABILITY_KEYBOARD,
	                        TRUE,
	                        NULL,
	                        NULL,
	                        NULL,
	                        NULL);

	if (status == CDK_GRAB_SUCCESS) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
break_window_map_event_cb (CtkWidget *widget,
			   CdkEvent  *event,
			   DrWright  *dr)
{
	grab_keyboard_on_window (ctk_widget_get_window (dr->break_window), ctk_get_current_event_time ());

        return FALSE;
}

static gboolean
maybe_change_state (DrWright *dr)
{
	gint elapsed_time;
	gint elapsed_idle_time;

	if (debug) {
		drw_timer_start (dr->idle_timer);
	}

	elapsed_time = drw_timer_elapsed (dr->timer) + dr->save_last_time;
	elapsed_idle_time = drw_timer_elapsed (dr->idle_timer);

	if (elapsed_time > dr->last_elapsed_time + dr->warn_time) {
		/* If the timeout is delayed by the amount of warning time, then
		 * we must have been suspended or stopped, so we just start
		 * over.
		 */
		dr->state = STATE_START;
	}

	switch (dr->state) {
	case STATE_START:
		if (dr->break_window) {
			ctk_widget_destroy (dr->break_window);
			dr->break_window = NULL;
		}

#ifndef HAVE_APP_INDICATOR
		set_status_icon (dr->icon, dr->neutral_bar);
#endif /* HAVE_APP_INDICATOR */

		dr->save_last_time = 0;

		drw_timer_start (dr->timer);
		drw_timer_start (dr->idle_timer);

		if (dr->enabled) {
			dr->state = STATE_RUNNING;
		}

		update_status (dr);
		stop_blinking (dr);
		break;

	case STATE_RUNNING:
	case STATE_WARN:
		if (elapsed_idle_time >= dr->break_time) {
			dr->state = STATE_BREAK_DONE_SETUP;
 		} else if (elapsed_time >= dr->type_time) {
			dr->state = STATE_BREAK_SETUP;
		} else if (dr->state != STATE_WARN
			   && elapsed_time >= dr->type_time - dr->warn_time) {
			dr->state = STATE_WARN;
			start_blinking (dr);
		}
		break;

	case STATE_BREAK_SETUP:
		/* Don't allow more than one break window to coexist, can happen
		 * if a break is manually enforced.
		 */
		if (dr->break_window) {
			dr->state = STATE_BREAK;
			break;
		}

		stop_blinking (dr);
#ifndef HAVE_APP_INDICATOR
		set_status_icon (dr->icon, dr->red_bar);
#endif /* HAVE_APP_INDICATOR */

		drw_timer_start (dr->timer);

		dr->break_window = drw_break_window_new ();

		g_signal_connect (dr->break_window, "map_event",
				  G_CALLBACK (break_window_map_event_cb),
				  dr);

		g_signal_connect (dr->break_window,
				  "done",
				  G_CALLBACK (break_window_done_cb),
				  dr);

		g_signal_connect (dr->break_window,
				  "postpone",
				  G_CALLBACK (break_window_postpone_cb),
				  dr);

		g_signal_connect (dr->break_window,
				  "destroy",
				  G_CALLBACK (break_window_destroy_cb),
				  dr);

		dr->secondary_break_windows = create_secondary_break_windows ();

		ctk_widget_show (dr->break_window);

		dr->save_last_time = elapsed_time;
		dr->state = STATE_BREAK;
		break;

	case STATE_BREAK:
		if (elapsed_time - dr->save_last_time >= dr->break_time) {
			dr->state = STATE_BREAK_DONE_SETUP;
		}
		break;

	case STATE_BREAK_DONE_SETUP:
		stop_blinking (dr);
#ifndef HAVE_APP_INDICATOR
		set_status_icon (dr->icon, dr->green_bar);
#endif /* HAVE_APP_INDICATOR */

		dr->state = STATE_BREAK_DONE;
		break;

	case STATE_BREAK_DONE:
		dr->state = STATE_START;
		if (dr->break_window) {
			ctk_widget_destroy (dr->break_window);
			dr->break_window = NULL;
		}
		break;
	}

	dr->last_elapsed_time = elapsed_time;

#ifdef HAVE_APP_INDICATOR
	update_app_indicator (dr);
#else
	update_icon (dr);
#endif /* HAVE_APP_INDICATOR */

	return TRUE;
}

static gboolean
update_status (DrWright *dr)
{
	gint       min;
	gchar     *str;
#ifdef HAVE_APP_INDICATOR
	CtkWidget *item;
#endif /* HAVE_APP_INDICATOR */

	if (!dr->enabled) {
#ifdef HAVE_APP_INDICATOR
		app_indicator_set_status (dr->indicator,
					  APP_INDICATOR_STATUS_PASSIVE);
#else
		ctk_status_icon_set_tooltip_text (dr->icon,
						  _("Disabled"));
#endif /* HAVE_APP_INDICATOR */
		return TRUE;
	}

	min = get_time_left (dr);

	if (min >= 1) {
#ifdef HAVE_APP_INDICATOR
		str = g_strdup_printf (_("Take a break now (next in %dm)"), min);
#else
		str = g_strdup_printf (ngettext("%d minute until the next break",
						"%d minutes until the next break",
						min), min);
#endif /* HAVE_APP_INDICATOR */
	} else {
#ifdef HAVE_APP_INDICATOR
		str = g_strdup_printf (_("Take a break now (next in less than one minute)"));
#else
		str = g_strdup_printf (_("Less than one minute until the next break"));
#endif /* HAVE_APP_INDICATOR */
	}

#ifdef HAVE_APP_INDICATOR
	item = ctk_ui_manager_get_widget (dr->ui_manager, "/Pop/TakeABreak");
	ctk_menu_item_set_label (CTK_MENU_ITEM (item), str);
#else
	ctk_status_icon_set_tooltip_text (dr->icon, str);
#endif /* HAVE_APP_INDICATOR */

	g_free (str);

	return TRUE;
}

static gint
get_time_left (DrWright *dr)
{
	gint elapsed_time;

	elapsed_time = drw_timer_elapsed (dr->timer);

	return floor (0.5 + (dr->type_time - elapsed_time - dr->save_last_time) / 60.0);
}

static void
activity_detected_cb (DrwMonitor *monitor,
		      DrWright   *dr)
{
	drw_timer_start (dr->idle_timer);
}

static void
gsettings_notify_cb (GSettings *settings,
		 gchar       *key,
		 gpointer     user_data)
{
	DrWright  *dr = user_data;
	CtkWidget *item;

	if (!strcmp (key, "type-time")) {
		dr->type_time = 60 * g_settings_get_int (settings, key);
		dr->warn_time = MIN (dr->type_time / 10, 5*60);

		dr->state = STATE_START;
	}
	else if (!strcmp (key, "break-time")) {
		dr->break_time = 60 * g_settings_get_int (settings, key);
		dr->state = STATE_START;
	}
	else if (!strcmp (key, "enabled")) {
		dr->enabled = g_settings_get_boolean (settings, key);
		dr->state = STATE_START;

		item = ctk_ui_manager_get_widget (dr->ui_manager,
						  "/Pop/TakeABreak");
		ctk_widget_set_sensitive (item, dr->enabled);

		update_status (dr);
	}

	maybe_change_state (dr);
}

static void
popup_break_cb (CtkAction *action, DrWright *dr)
{
	if (dr->enabled) {
		dr->state = STATE_BREAK_SETUP;
		maybe_change_state (dr);
	}
}

static void
popup_preferences_cb (CtkAction *action, DrWright *dr)
{
	CdkScreen *screen;
	GError    *error = NULL;
	CtkWidget *menu;

	menu = ctk_ui_manager_get_widget (dr->ui_manager, "/Pop");
	screen = ctk_widget_get_screen (menu);

	if (!cafe_cdk_spawn_command_line_on_screen (screen, "cafe-keyboard-properties --typing-break", &error)) {
		CtkWidget *error_dialog;

		error_dialog = ctk_message_dialog_new (NULL, 0,
						       CTK_MESSAGE_ERROR,
						       CTK_BUTTONS_CLOSE,
						       _("Unable to bring up the typing break properties dialog with the following error: %s"),
						       error->message);
		g_signal_connect (error_dialog,
				  "response",
				  G_CALLBACK (ctk_widget_destroy), NULL);
		ctk_window_set_resizable (CTK_WINDOW (error_dialog), FALSE);
		ctk_widget_show (error_dialog);

		g_error_free (error);
	}
}

static void
popup_about_cb (CtkAction *action, DrWright *dr)
{
	gint   i;
	gchar *authors[] = {
		N_("Written by Richard Hult <richard@imendio.com>"),
		N_("Eye candy added by Anders Carlsson"),
		NULL
	};

	for (i = 0; authors [i]; i++)
		authors [i] = _(authors [i]);

	ctk_show_about_dialog (NULL,
			       "authors", authors,
			       "comments",  _("A computer break reminder."),
			       "logo-icon-name", "cafe-typing-monitor",
			       "translator-credits", _("translator-credits"),
			       "version", VERSION,
			       NULL);
}

#ifndef HAVE_APP_INDICATOR
static void
popup_menu_cb (CtkWidget *widget,
	       guint      button,
	       guint      activate_time,
	       DrWright  *dr)
{
	CtkWidget *menu;

	menu = ctk_ui_manager_get_widget (dr->ui_manager, "/Pop");

	ctk_menu_popup (CTK_MENU (menu),
			NULL,
			NULL,
			ctk_status_icon_position_menu,
			dr->icon,
			0,
			ctk_get_current_event_time ());
}
#endif /* HAVE_APP_INDICATOR */

static void
break_window_done_cb (CtkWidget *window,
		      DrWright  *dr)
{
	ctk_widget_destroy (dr->break_window);

	dr->state = STATE_BREAK_DONE_SETUP;
	dr->break_window = NULL;

	update_status (dr);
	maybe_change_state (dr);
}

static void
break_window_postpone_cb (CtkWidget *window,
			  DrWright  *dr)
{
	gint elapsed_time;

	ctk_widget_destroy (dr->break_window);

	dr->state = STATE_RUNNING;
	dr->break_window = NULL;

	elapsed_time = drw_timer_elapsed (dr->timer);

	if (elapsed_time + dr->save_last_time >= dr->type_time) {
		/* Typing time has expired, but break was postponed.
		 * We'll warn again in (elapsed * sqrt (typing_time))^2 */
		gfloat postpone_time = (((float) elapsed_time) / dr->break_time)
					* sqrt (dr->type_time);
		postpone_time *= postpone_time;
		dr->save_last_time = dr->type_time - MAX (dr->warn_time, (gint) postpone_time);
	}

	drw_timer_start (dr->timer);
	maybe_change_state (dr);
	update_status (dr);
#ifdef HAVE_APP_INDICATOR
	update_app_indicator (dr);
#else
	update_icon (dr);
#endif /* HAVE_APP_INDICATOR */
}

static void
break_window_destroy_cb (CtkWidget *window,
			 DrWright  *dr)
{
	GList *l;

	for (l = dr->secondary_break_windows; l; l = l->next) {
		ctk_widget_destroy (l->data);
	}

	g_list_free (dr->secondary_break_windows);
	dr->secondary_break_windows = NULL;
}

#ifdef HAVE_APP_INDICATOR
static void
init_app_indicator (DrWright *dr)
{
	CtkWidget *indicator_menu;

	dr->indicator =
		app_indicator_new_with_path ("typing-break-indicator",
					     TYPING_MONITOR_ACTIVE_ICON,
					     APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
					     IMAGEDIR);
	if (dr->enabled) {
		app_indicator_set_status (dr->indicator,
					  APP_INDICATOR_STATUS_ACTIVE);
	} else {
		app_indicator_set_status (dr->indicator,
					  APP_INDICATOR_STATUS_PASSIVE);
	}

	indicator_menu = ctk_ui_manager_get_widget (dr->ui_manager, "/Pop");
	app_indicator_set_menu (dr->indicator, CTK_MENU (indicator_menu));
	app_indicator_set_attention_icon (dr->indicator, TYPING_MONITOR_ATTENTION_ICON);

	update_status (dr);
	update_app_indicator (dr);
}
#else
static void
init_tray_icon (DrWright *dr)
{
	CdkPixbuf *pixbuf;

	pixbuf = cdk_pixbuf_get_from_surface (dr->neutral_bar, 0, 0,
					      cairo_image_surface_get_width (dr->neutral_bar),
					      cairo_image_surface_get_height (dr->neutral_bar));

	dr->icon = ctk_status_icon_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);

	update_status (dr);
	update_icon (dr);

	g_signal_connect (dr->icon,
			  "popup_menu",
			  G_CALLBACK (popup_menu_cb),
			  dr);
}
#endif /* HAVE_APP_INDICATOR */

static GList *
create_secondary_break_windows (void)
{
	CdkDisplay *display;
	CdkScreen  *screen;
	CtkWidget  *window;
	GList      *windows = NULL;
	gint        scale;

	display = cdk_display_get_default ();

	screen = cdk_display_get_default_screen (display);

	if (screen != cdk_screen_get_default ()) {
		/* Handled by DrwBreakWindow. */

		window = ctk_window_new (CTK_WINDOW_POPUP);

		windows = g_list_prepend (windows, window);
		scale = ctk_widget_get_scale_factor (CTK_WIDGET (window));

		ctk_window_set_screen (CTK_WINDOW (window), screen);

		ctk_window_set_default_size (CTK_WINDOW (window),
					     WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale,
					     HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale);

		ctk_widget_set_app_paintable (CTK_WIDGET (window), TRUE);
		drw_setup_background (CTK_WIDGET (window));
		ctk_window_stick (CTK_WINDOW (window));
		ctk_widget_show (window);
	}

	return windows;
}

DrWright *
drwright_new (void)
{
	DrWright  *dr;
	CtkWidget *item;
	GSettings *settings;
	CtkActionGroup *action_group;

	static const char ui_description[] =
	  "<ui>"
	  "  <popup name='Pop'>"
	  "    <menuitem action='Preferences'/>"
	  "    <menuitem action='About'/>"
	  "    <separator/>"
	  "    <menuitem action='TakeABreak'/>"
	  "  </popup>"
	  "</ui>";

        dr = g_new0 (DrWright, 1);

	settings = g_settings_new (TYPING_BREAK_SCHEMA);

	g_signal_connect (settings, "changed", G_CALLBACK (gsettings_notify_cb), dr);

	dr->type_time = 60 * g_settings_get_int (settings, "type-time");

	dr->warn_time = MIN (dr->type_time / 12, 60*3);

	dr->break_time = 60 * g_settings_get_int (settings, "break-time");

	dr->enabled = g_settings_get_boolean (settings, "enabled");

	if (debug) {
		setup_debug_values (dr);
	}

	dr->ui_manager = ctk_ui_manager_new ();

	action_group = ctk_action_group_new ("MenuActions");
	ctk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	ctk_action_group_add_actions (action_group, actions, G_N_ELEMENTS (actions), dr);
	ctk_ui_manager_insert_action_group (dr->ui_manager, action_group, 0);
	ctk_ui_manager_add_ui_from_string (dr->ui_manager, ui_description, -1, NULL);

	item = ctk_ui_manager_get_widget (dr->ui_manager, "/Pop/TakeABreak");
	ctk_widget_set_sensitive (item, dr->enabled);

	dr->timer = drw_timer_new ();
	dr->idle_timer = drw_timer_new ();

	dr->state = STATE_START;

	dr->monitor = drw_monitor_new ();

	g_signal_connect (dr->monitor,
			  "activity",
			  G_CALLBACK (activity_detected_cb),
			  dr);

#ifdef HAVE_APP_INDICATOR
	init_app_indicator (dr);
#else
	dr->neutral_bar = cairo_image_surface_create_from_png (IMAGEDIR "/bar.png");
	dr->red_bar = cairo_image_surface_create_from_png (IMAGEDIR "/bar-red.png");
	dr->green_bar = cairo_image_surface_create_from_png (IMAGEDIR "/bar-green.png");
	dr->disabled_bar = cairo_image_surface_create_from_png (IMAGEDIR "/bar-disabled.png");

	init_tray_icon (dr);
#endif /* HAVE_APP_INDICATOR */

	g_timeout_add_seconds (12,
			       (GSourceFunc) update_status,
			       dr);

	g_timeout_add_seconds (1,
			       (GSourceFunc) maybe_change_state,
			       dr);

	return dr;
}
