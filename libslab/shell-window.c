/*
 * This file is part of libslab.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libslab is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libslab is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "shell-window.h"

#include <ctk/ctk.h>
#include <gdk/gdkx.h>

#include "app-resizer.h"

static void shell_window_handle_size_request (CtkWidget * widget, CtkRequisition * requisition,
	AppShellData * data);

gboolean shell_window_paint_window (CtkWidget * widget, cairo_t * cr, gpointer data);

#define SHELL_WINDOW_BORDER_WIDTH 6

G_DEFINE_TYPE (ShellWindow, shell_window, CTK_TYPE_FRAME);

static void
shell_window_class_init (ShellWindowClass * klass)
{
}

static void
shell_window_init (ShellWindow * window)
{
	window->_hbox = NULL;
	window->_left_pane = NULL;
	window->_right_pane = NULL;
}

CtkWidget *
shell_window_new (AppShellData * app_data)
{
	ShellWindow *window = g_object_new (SHELL_WINDOW_TYPE, NULL);

	ctk_widget_set_app_paintable (CTK_WIDGET (window), TRUE);
	ctk_frame_set_shadow_type(CTK_FRAME(window), CTK_SHADOW_NONE);

	window->_hbox = CTK_BOX (ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0));
	ctk_container_add (CTK_CONTAINER (window), CTK_WIDGET (window->_hbox));

	g_signal_connect (G_OBJECT (window), "draw", G_CALLBACK (shell_window_paint_window),
		NULL);

/* FIXME add some replacement for CTK+3, or just remove this code? */
#if 0
	window->resize_handler_id =
		g_signal_connect (G_OBJECT (window), "size-request",
		G_CALLBACK (shell_window_handle_size_request), app_data);
#endif

	return CTK_WIDGET (window);
}

void
shell_window_clear_resize_handler (ShellWindow * win)
{
	if (win->resize_handler_id)
	{
		g_signal_handler_disconnect (win, win->resize_handler_id);
		win->resize_handler_id = 0;
	}
}

/* We want the window to come up with proper runtime calculated width ( ie taking into account font size, locale, ...) so
   we can't hard code a size. But since ScrolledWindow returns basically zero for it's size request we need to
   grab the "real" desired width. Once it's shown though we want to allow the user to size down if they want too, so
   we unhook this function
*/
static void
shell_window_handle_size_request (CtkWidget * widget, CtkRequisition * requisition,
	AppShellData * app_data)
{
	gint height;
	CtkRequisition child_requisiton;

	ctk_widget_get_preferred_size (CTK_WIDGET (APP_RESIZER (app_data->category_layout)->child), &child_requisiton, NULL);

	requisition->width += child_requisiton.width;

	/* use the left side as a minimum height, if the right side is taller,
	   use it up to SIZING_HEIGHT_PERCENT of the screen height
	*/
	height = child_requisiton.height + 10;
	if (height > requisition->height)
	{
		requisition->height =
			MIN (((gfloat) HeightOfScreen (gdk_x11_screen_get_xscreen (gdk_screen_get_default ())) * SIZING_HEIGHT_PERCENT), height);
	}
}

void
shell_window_set_contents (ShellWindow * shell, CtkWidget * left_pane, CtkWidget * right_pane)
{
	shell->_left_pane = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_set_margin_top (CTK_WIDGET (shell->_left_pane), 15);
	ctk_widget_set_margin_bottom (CTK_WIDGET (shell->_left_pane), 15);
	ctk_widget_set_margin_start (CTK_WIDGET (shell->_left_pane), 15);
	ctk_widget_set_margin_end (CTK_WIDGET (shell->_left_pane), 15);

	shell->_right_pane = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);

	ctk_box_pack_start (shell->_hbox, shell->_left_pane, FALSE, FALSE, 0);
	ctk_box_pack_start (shell->_hbox, shell->_right_pane, TRUE, TRUE, 0);	/* this one takes any extra space */

	ctk_container_add (CTK_CONTAINER (shell->_left_pane), left_pane);
	ctk_container_add (CTK_CONTAINER (shell->_right_pane), right_pane);
}

gboolean
shell_window_paint_window (CtkWidget * widget, cairo_t * cr, gpointer data)
{
	CtkWidget *left_pane;
	CtkAllocation allocation;

	left_pane = SHELL_WINDOW (widget)->_left_pane;

	ctk_widget_get_allocation (left_pane, &allocation);

	/* draw left pane background */
	ctk_paint_flat_box (
		ctk_widget_get_style (widget),
		cr,
		ctk_widget_get_state (widget),
		CTK_SHADOW_NONE,
		widget,
		"",
		allocation.x,
		allocation.y,
		allocation.width,
		allocation.height);

	return FALSE;
}
