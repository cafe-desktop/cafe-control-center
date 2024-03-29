/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Jens Granseuer <jensgr@gmx.net>
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

enum {
	COL_THUMBNAIL,
	COL_LABEL,
	COL_NAME,
	NUM_COLS
};

typedef enum {
	THEME_TYPE_CTK,
	THEME_TYPE_WINDOW,
	THEME_TYPE_ICON,
	THEME_TYPE_META,
	THEME_TYPE_CURSOR
} ThemeType;

gboolean theme_is_writable(const gpointer theme);
gboolean theme_delete(const gchar* name, ThemeType type);

gboolean theme_model_iter_last(CtkTreeModel* model, CtkTreeIter* iter);
gboolean theme_find_in_model(CtkTreeModel* model, const gchar* name, CtkTreeIter* iter);

void theme_install_file(CtkWindow* parent, const gchar* path);
gboolean packagekit_available(void);
