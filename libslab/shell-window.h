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

#ifndef __SHELL_WINDOW_H__
#define __SHELL_WINDOW_H__

#include <glib.h>
#include <ctk/ctk.h>

#include "app-shell.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHELL_WINDOW_TYPE            (shell_window_get_type ())
#define SHELL_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_WINDOW_TYPE, ShellWindow))
#define SHELL_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_WINDOW_TYPE, ShellWindowClass))
#define IS_SHELL_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_WINDOW_TYPE))
#define IS_SHELL_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_WINDOW_TYPE))
#define SHELL_WINDOW_GET_CLASS(obj)  (G_TYPE_CHECK_GET_CLASS ((obj), SHELL_WINDOW_TYPE, ShellWindowClass))

typedef struct _ShellWindow ShellWindow;
typedef struct _ShellWindowClass ShellWindowClass;

struct _ShellWindow
{
	CtkFrame frame;

	CtkBox *_hbox;
	CtkWidget *_left_pane;
	CtkWidget *_right_pane;

	gulong resize_handler_id;
};

struct _ShellWindowClass
{
	CtkFrameClass parent_class;
};

GType shell_window_get_type (void);
CtkWidget *shell_window_new (AppShellData * app_data);
void shell_window_set_contents (ShellWindow * window, CtkWidget * left_pane,
	CtkWidget * right_pane);
void shell_window_clear_resize_handler (ShellWindow * win);

#ifdef __cplusplus
}
#endif
#endif /* __SHELL_WINDOW_H__ */
