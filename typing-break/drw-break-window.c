/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002      CodeFactory AB
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
#include <math.h>
#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>
#include <cdk/cdkkeysyms.h>
#include <gio/gio.h>

#ifdef HAVE_KANBERRA_CTK
#include <kanberra-gtk.h>
#endif

#include "drwright.h"
#include "drw-utils.h"
#include "drw-break-window.h"
#include "drw-timer.h"

struct _DrwBreakWindowPrivate {
	CtkWidget *clock_label;
	CtkWidget *break_label;
	CtkWidget *image;

	CtkWidget *postpone_entry;
	CtkWidget *postpone_button;

	DrwTimer  *timer;

	gint       break_time;
	gchar     *break_text;
	guint      clock_timeout_id;
	guint      postpone_timeout_id;
	guint      postpone_sensitize_id;
};

#define POSTPONE_CANCEL 30

/* Signals */
enum {
	DONE,
	POSTPONE,
	LAST_SIGNAL
};

static void         drw_break_window_finalize      (GObject             *object);
static void         drw_break_window_dispose       (GObject             *object);
static gboolean     postpone_sensitize_cb          (DrwBreakWindow      *window);
static gboolean     clock_timeout_cb               (DrwBreakWindow      *window);
static void         postpone_clicked_cb            (CtkWidget           *button,
						    CtkWidget           *window);
static gboolean     label_draw_event_cb            (CtkLabel            *label,
						    cairo_t             *cr,
						    gpointer             user_data);
static void         label_size_allocate_cb         (CtkLabel            *label,
						    CdkRectangle        *allocation,
						    gpointer             user_data);

G_DEFINE_TYPE_WITH_PRIVATE (DrwBreakWindow, drw_break_window, CTK_TYPE_WINDOW)

static guint signals[LAST_SIGNAL] = { 0 };

static void
drw_break_window_class_init (DrwBreakWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = drw_break_window_finalize;
	object_class->dispose = drw_break_window_dispose;

	signals[POSTPONE] =
		g_signal_new ("postpone",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[DONE] =
		g_signal_new ("done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
drw_break_window_init (DrwBreakWindow *window)
{
	DrwBreakWindowPrivate *priv;
	CtkWidget             *vbox;
	CtkWidget             *hbox;
	gchar                 *str;
	CtkWidget             *outer_vbox;
	CtkWidget             *button_box;
	gboolean               allow_postpone;

	gint                   root_monitor = 0;
	CdkScreen             *screen = NULL;
	CdkDisplay            *display;
	CdkRectangle           monitor;
	gint                   right_padding;
	gint                   bottom_padding;
	gint                   scale;
	GSettings             *settings;

	priv = drw_break_window_get_instance_private (window);
	window->priv = priv;

	settings = g_settings_new (TYPING_BREAK_SCHEMA);

	priv->break_time = 60 * g_settings_get_int (settings, "break-time");

	allow_postpone = g_settings_get_boolean (settings, "allow-postpone");
	g_object_unref (settings);

	ctk_window_set_keep_above (CTK_WINDOW (window), TRUE);
	ctk_window_fullscreen (CTK_WINDOW (window));
	ctk_window_set_modal (CTK_WINDOW (window), TRUE);

	screen = cdk_screen_get_default ();
	display = cdk_screen_get_display (screen);
	scale = ctk_widget_get_scale_factor (CTK_WIDGET (window));

	cdk_monitor_get_geometry (cdk_display_get_monitor (display, root_monitor), &monitor);

	ctk_window_set_default_size (CTK_WINDOW (window),
				     WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale,
				     HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale);

	ctk_window_set_decorated (CTK_WINDOW (window), FALSE);
	ctk_widget_set_app_paintable (CTK_WIDGET (window), TRUE);
	drw_setup_background (CTK_WIDGET (window));

	right_padding = WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale - monitor.width - monitor.x;
	bottom_padding = HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale - monitor.height - monitor.y;

	outer_vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_set_hexpand (outer_vbox, TRUE);
	ctk_widget_set_vexpand (outer_vbox, TRUE);
	ctk_widget_set_margin_top (outer_vbox, monitor.y);
	ctk_widget_set_margin_bottom (outer_vbox, bottom_padding);
	ctk_widget_set_margin_start (outer_vbox, monitor.x);
	ctk_widget_set_margin_end (outer_vbox, right_padding);
	ctk_widget_show (outer_vbox);

	ctk_container_add (CTK_CONTAINER (window), outer_vbox);

	if (allow_postpone) {
		button_box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
		ctk_widget_show (button_box);

		ctk_container_set_border_width (CTK_CONTAINER (button_box), 12);

		priv->postpone_button = ctk_button_new_with_mnemonic (_("_Postpone Break"));
		ctk_widget_show (priv->postpone_button);

		ctk_widget_set_sensitive (priv->postpone_button, FALSE);

		if (priv->postpone_sensitize_id) {
			g_source_remove (priv->postpone_sensitize_id);
		}

		priv->postpone_sensitize_id = g_timeout_add_seconds (5,
								     (GSourceFunc) postpone_sensitize_cb,
								     window);

		g_signal_connect (priv->postpone_button,
				  "clicked",
				  G_CALLBACK (postpone_clicked_cb),
				  window);

		ctk_box_pack_end (CTK_BOX (button_box), priv->postpone_button, FALSE, TRUE, 0);

		priv->postpone_entry = ctk_entry_new ();
		ctk_entry_set_has_frame (CTK_ENTRY (priv->postpone_entry), FALSE);

		ctk_box_pack_end (CTK_BOX (button_box), priv->postpone_entry, FALSE, TRUE, 4);

		ctk_box_pack_end (CTK_BOX (outer_vbox), button_box, FALSE, TRUE, 0);
	}

	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_set_halign (vbox, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (vbox, CTK_ALIGN_CENTER);
	ctk_widget_show (vbox);

	ctk_box_pack_start (CTK_BOX (outer_vbox), vbox, TRUE, TRUE, 0);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_widget_show (hbox);
	ctk_box_pack_start (CTK_BOX (vbox), hbox, TRUE, FALSE, 0);

	priv->image = ctk_image_new_from_icon_name ("process-stop", CTK_ICON_SIZE_DIALOG);
	ctk_widget_set_halign (priv->image, CTK_ALIGN_END);
	ctk_widget_set_valign (priv->image, CTK_ALIGN_CENTER);
	ctk_widget_show (priv->image);
	ctk_box_pack_start (CTK_BOX (hbox), priv->image, TRUE, TRUE, 8);

	priv->break_label = ctk_label_new (NULL);
	ctk_widget_show (priv->break_label);

	g_signal_connect (priv->break_label,
			  "draw",
			  G_CALLBACK (label_draw_event_cb),
			  NULL);

	g_signal_connect_after (priv->break_label,
				"size-allocate",
				G_CALLBACK (label_size_allocate_cb),
				NULL);

	str = g_strdup_printf ("<span size=\"xx-large\" foreground=\"white\"><b>%s</b></span>",
			       _("Take a break!"));
	ctk_label_set_markup (CTK_LABEL (priv->break_label), str);
	g_free (str);

	ctk_box_pack_start (CTK_BOX (hbox), priv->break_label, FALSE, FALSE, 12);


	priv->clock_label = ctk_label_new (NULL);
	ctk_widget_show (priv->clock_label);
	ctk_box_pack_start (CTK_BOX (vbox), priv->clock_label, TRUE, TRUE, 8);

	g_signal_connect (priv->clock_label,
			  "draw",
			  G_CALLBACK (label_draw_event_cb),
			  NULL);

	g_signal_connect_after (priv->clock_label,
				"size-allocate",
				G_CALLBACK (label_size_allocate_cb),
				NULL);

	ctk_window_stick (CTK_WINDOW (window));

	priv->timer = drw_timer_new ();

	/* Make sure we have a valid time label from the start. */
	clock_timeout_cb (window);

	priv->clock_timeout_id = g_timeout_add (1000,
						(GSourceFunc) clock_timeout_cb,
						window);
#ifdef HAVE_KANBERRA_CTK
	ka_context_play (ka_gtk_context_get (), 0, CA_PROP_EVENT_ID, "desktop-screen-lock", NULL);
#endif
}

static void
drw_break_window_finalize (GObject *object)
{
	DrwBreakWindow        *window = DRW_BREAK_WINDOW (object);
	DrwBreakWindowPrivate *priv;

	priv = window->priv;

	if (priv->clock_timeout_id != 0) {
		g_source_remove (priv->clock_timeout_id);
	}

	if (priv->postpone_timeout_id != 0) {
		g_source_remove (priv->postpone_timeout_id);
	}

	if (priv->postpone_sensitize_id != 0) {
		g_source_remove (priv->postpone_sensitize_id);
	}

	window->priv = NULL;

	if (G_OBJECT_CLASS (drw_break_window_parent_class)->finalize) {
		(* G_OBJECT_CLASS (drw_break_window_parent_class)->finalize) (object);
	}
}

static void
drw_break_window_dispose (GObject *object)
{
	DrwBreakWindow        *window = DRW_BREAK_WINDOW (object);
	DrwBreakWindowPrivate *priv;

	priv = window->priv;

	if (priv->timer) {
		drw_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	if (priv->clock_timeout_id != 0) {
		g_source_remove (priv->clock_timeout_id);
		priv->clock_timeout_id = 0;
	}

	if (priv->postpone_timeout_id != 0) {
		g_source_remove (priv->postpone_timeout_id);
		priv->postpone_timeout_id = 0;
	}

	if (priv->postpone_sensitize_id != 0) {
		g_source_remove (priv->postpone_sensitize_id);
	}

	if (G_OBJECT_CLASS (drw_break_window_parent_class)->dispose) {
		(* G_OBJECT_CLASS (drw_break_window_parent_class)->dispose) (object);
	}
}

CtkWidget *
drw_break_window_new (void)
{
	GObject *object;

	object = g_object_new (DRW_TYPE_BREAK_WINDOW,
			       "type", CTK_WINDOW_POPUP,
			       "skip-taskbar-hint", TRUE,
			       "skip-pager-hint", TRUE,
			       "focus-on-map", TRUE,
			       NULL);

	return CTK_WIDGET (object);
}

static gboolean
postpone_sensitize_cb (DrwBreakWindow *window)
{
	DrwBreakWindowPrivate *priv;

	priv = window->priv;

	ctk_widget_set_sensitive (priv->postpone_button, TRUE);

	priv->postpone_sensitize_id = 0;
	return FALSE;
}

static gboolean
clock_timeout_cb (DrwBreakWindow *window)
{
	DrwBreakWindowPrivate *priv;
	gchar                 *txt;
	gint                   minutes;
	gint                   seconds;

	g_return_val_if_fail (DRW_IS_BREAK_WINDOW (window), FALSE);

	priv = window->priv;

	seconds = 1 + priv->break_time - drw_timer_elapsed (priv->timer);
	seconds = MAX (0, seconds);

	if (seconds == 0) {
		/* Zero this out so the finalizer doesn't try to remove the
		 * source, which would be done in the timeout callback ==
		 * no-no.
		 */
		priv->clock_timeout_id = 0;

#ifdef HAVE_KANBERRA_CTK
		ka_context_play (ka_gtk_context_get (), 0, CA_PROP_EVENT_ID, "alarm-clock-elapsed", NULL);
#endif
		g_signal_emit (window, signals[DONE], 0, NULL);

		return FALSE;
	}

	minutes = seconds / 60;
	seconds -= minutes * 60;

	txt = g_strdup_printf ("<span size=\"25000\" foreground=\"white\"><b>%d:%02d</b></span>",
			       minutes,
			       seconds);
	ctk_label_set_markup (CTK_LABEL (priv->clock_label), txt);
	g_free (txt);

	return TRUE;
}

static void
postpone_entry_activate_cb (CtkWidget      *entry,
			  DrwBreakWindow *window)
{
	const gchar *str;
	gchar *phrase;
	GSettings *settings = g_settings_new (TYPING_BREAK_SCHEMA);

	str = ctk_entry_get_text (CTK_ENTRY (entry));

	phrase = g_settings_get_string (settings, "unlock-phrase");
	g_object_unref (settings);

	if (!strcmp (str, phrase)) {
		g_signal_emit (window, signals[POSTPONE], 0, NULL);
		g_free (phrase);
		return;
	}

	g_free (phrase);
	ctk_entry_set_text (CTK_ENTRY (entry), "");
}

static gboolean
grab_on_window (CdkWindow *window,
		guint32    activate_time)
{
	CdkDisplay *display;
	CdkSeat *seat;

	display = cdk_window_get_display (window);
	seat = cdk_display_get_default_seat (display);

	return (cdk_seat_grab (seat,
	                       window,
	                       CDK_SEAT_CAPABILITY_ALL,
	                       TRUE,
	                       NULL,
	                       NULL,
	                       NULL,
	                       NULL) == CDK_GRAB_SUCCESS);
}

static gboolean
postpone_cancel_cb (DrwBreakWindow *window)
{
	DrwBreakWindowPrivate *priv;

	priv = window->priv;

	ctk_entry_set_text (CTK_ENTRY (priv->postpone_entry), "");
	ctk_widget_hide (priv->postpone_entry);

	priv->postpone_timeout_id = 0;

	return FALSE;
}

static gboolean
postpone_entry_key_press_event_cb (CtkEntry       *entry,
				   CdkEventKey    *event,
				   DrwBreakWindow *window)
{
	DrwBreakWindowPrivate *priv;

	priv = window->priv;

	if (event->keyval == CDK_KEY_Escape) {
		if (priv->postpone_timeout_id) {
			g_source_remove (priv->postpone_timeout_id);
		}

		postpone_cancel_cb (window);

		return TRUE;
	}

	g_source_remove (priv->postpone_timeout_id);

	priv->postpone_timeout_id = g_timeout_add_seconds (POSTPONE_CANCEL, (GSourceFunc) postpone_cancel_cb, window);

	return FALSE;
}

static void
postpone_clicked_cb (CtkWidget *button,
		     CtkWidget *window)
{
	DrwBreakWindow        *bw = DRW_BREAK_WINDOW (window);
	DrwBreakWindowPrivate *priv = bw->priv;
	gchar                 *phrase;

	/* Disable the phrase for now. */
	phrase = NULL; /*g_settings_get_string (settings, "unlock-phrase");*/

	if (!phrase || !phrase[0]) {
		g_signal_emit (window, signals[POSTPONE], 0, NULL);
		return;
	}

	if (ctk_widget_get_visible (priv->postpone_entry)) {
		ctk_widget_activate (priv->postpone_entry);
		return;
	}

	ctk_widget_show (priv->postpone_entry);

	priv->postpone_timeout_id = g_timeout_add_seconds (POSTPONE_CANCEL, (GSourceFunc) postpone_cancel_cb, bw);

	grab_on_window (ctk_widget_get_window (priv->postpone_entry),  ctk_get_current_event_time ());

	ctk_widget_grab_focus (priv->postpone_entry);

	g_signal_connect (priv->postpone_entry,
			  "activate",
			  G_CALLBACK (postpone_entry_activate_cb),
			  bw);

	g_signal_connect (priv->postpone_entry,
			  "key_press_event",
			  G_CALLBACK (postpone_entry_key_press_event_cb),
			  bw);
}

static void
get_layout_location (CtkLabel *label,
                     gint     *xp,
                     gint     *yp)
{
	CtkWidget      *widget;
	CtkAllocation  widget_allocation;
	CtkRequisition widget_requisition;
	gfloat         xalign, yalign;
	gint           x, y;
	gint           xpad, ypad;
	gint           margin_start, margin_end, margin_top, margin_bottom;
	gint           scale;

	widget = CTK_WIDGET (label);

	scale = ctk_widget_get_scale_factor (widget);
	xalign = ctk_label_get_xalign (CTK_LABEL (label));
	yalign = ctk_label_get_yalign (CTK_LABEL (label));
	margin_start = ctk_widget_get_margin_start (widget);
	margin_end = ctk_widget_get_margin_end (widget);
	margin_top = ctk_widget_get_margin_top (widget);
	margin_bottom = ctk_widget_get_margin_bottom (widget);

	xpad = margin_start + margin_end;
	ypad = margin_top + margin_bottom;

	ctk_widget_get_allocation (widget, &widget_allocation);
	ctk_widget_get_preferred_size (widget, &widget_requisition, NULL);
	widget_allocation.x /= scale;
	widget_allocation.y /= scale;
	widget_requisition.width /= scale;
	widget_requisition.height /= scale;

	if (ctk_widget_get_direction (widget) != CTK_TEXT_DIR_LTR)
		xalign = 1.0 - xalign;

	x = floor (widget_allocation.x + (int)xpad
		   + ((widget_allocation.width - widget_requisition.width - 1) * xalign)
		   + 0.5);

	y = floor (widget_allocation.y + (int)ypad
		   + ((widget_allocation.height - widget_requisition.height - 1) * yalign)
		   + 0.5);

	if (xp) {
		*xp = x;
	}

	if (yp) {
		*yp = y;
	}
}

static gboolean
label_draw_event_cb (CtkLabel *label,
                     cairo_t  *cr,
                     gpointer  user_data)
{
	gint       x, y;
	CtkWidget *widget;

	get_layout_location (label, &x, &y);

	widget = CTK_WIDGET (label);

	pango_cairo_show_layout (cr, ctk_label_get_layout (label));

	ctk_paint_layout (ctk_widget_get_style (widget),
			  cr,
			  ctk_widget_get_state (widget),
			  FALSE,
			  widget,
			  "label",
			  x, y,
			  ctk_label_get_layout (label));

	return TRUE;
}

static void
label_size_allocate_cb (CtkLabel     *label,
			CdkRectangle *allocation,
			gpointer      user_data)
{
	allocation->width += 1;
	allocation->height += 1;
}
