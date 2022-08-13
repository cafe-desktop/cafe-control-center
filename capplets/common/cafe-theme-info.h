/* cafe-theme-info.h - CAFE Theme information

   Copyright (C) 2002 Jonathan Blandford <jrb@gnome.org>
   All rights reserved.

   This file is part of the Cafe Library.

   The Cafe Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Cafe Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Cafe Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef CAFE_THEME_INFO_H
#define CAFE_THEME_INFO_H

#include <glib.h>
#include <gio/gio.h>
#include <ctk/ctk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>

typedef enum {
	CAFE_THEME_TYPE_METATHEME,
	CAFE_THEME_TYPE_ICON,
	CAFE_THEME_TYPE_CURSOR,
	CAFE_THEME_TYPE_REGULAR
} CafeThemeType;

typedef enum {
	CAFE_THEME_CHANGE_CREATED,
	CAFE_THEME_CHANGE_DELETED,
	CAFE_THEME_CHANGE_CHANGED
} CafeThemeChangeType;

typedef enum {
	CAFE_THEME_CROMA = 1 << 0,
	CAFE_THEME_GTK_2 = 1 << 1,
	CAFE_THEME_GTK_2_KEYBINDING = 1 << 2
} CafeThemeElement;

typedef struct _CafeThemeCommonInfo CafeThemeCommonInfo;
typedef struct _CafeThemeCommonInfo CafeThemeIconInfo;
struct _CafeThemeCommonInfo
{
	CafeThemeType type;
	gchar* path;
	gchar* name;
	gchar* readable_name;
	gint priority;
	gboolean hidden;
};

typedef struct _CafeThemeInfo CafeThemeInfo;
struct _CafeThemeInfo
{
	CafeThemeType type;
	gchar* path;
	gchar* name;
	gchar* readable_name;
	gint priority;
	gboolean hidden;

	guint has_ctk : 1;
	guint has_keybinding : 1;
	guint has_croma : 1;
};

typedef struct _CafeThemeCursorInfo CafeThemeCursorInfo;
struct _CafeThemeCursorInfo {
	CafeThemeType type;
	gchar* path;
	gchar* name;
	gchar* readable_name;
	gint priority;
	gboolean hidden;

	GArray* sizes;
	GdkPixbuf* thumbnail;
};

typedef struct _CafeThemeMetaInfo CafeThemeMetaInfo;
struct _CafeThemeMetaInfo {
	CafeThemeType type;
	gchar* path;
	gchar* name;
	gchar* readable_name;
	gint priority;
	gboolean hidden;

	gchar* comment;
	gchar* icon_file;

	gchar* ctk_theme_name;
	gchar* ctk_color_scheme;
	gchar* croma_theme_name;
	gchar* icon_theme_name;
	gchar* notification_theme_name;
	gchar* sound_theme_name;
	gchar* cursor_theme_name;
	guint cursor_size;

	gchar* application_font;
	gchar* documents_font;
	gchar* desktop_font;
	gchar* windowtitle_font;
	gchar* monospace_font;
	gchar* background_image;
};

enum {
	COLOR_FG,
	COLOR_BG,
	COLOR_TEXT,
	COLOR_BASE,
	COLOR_SELECTED_FG,
	COLOR_SELECTED_BG,
	COLOR_TOOLTIP_FG,
	COLOR_TOOLTIP_BG,
	NUM_SYMBOLIC_COLORS
};

typedef void (*ThemeChangedCallback) (CafeThemeCommonInfo* theme, CafeThemeChangeType change_type, CafeThemeElement element_type, gpointer user_data);

#define CAFE_THEME_ERROR cafe_theme_info_error_quark()

enum {
	CAFE_THEME_ERROR_GTK_THEME_NOT_AVAILABLE = 1,
	CAFE_THEME_ERROR_WM_THEME_NOT_AVAILABLE,
	CAFE_THEME_ERROR_ICON_THEME_NOT_AVAILABLE,
	CAFE_THEME_ERROR_GTK_ENGINE_NOT_AVAILABLE,
	CAFE_THEME_ERROR_UNKNOWN
};


/* GTK/Croma/keybinding Themes */
CafeThemeInfo     *cafe_theme_info_new                   (void);
void                cafe_theme_info_free                  (CafeThemeInfo     *theme_info);
CafeThemeInfo     *cafe_theme_info_find                  (const gchar        *theme_name);
GList              *cafe_theme_info_find_by_type          (guint               elements);
GQuark              cafe_theme_info_error_quark           (void);
gchar              *ctk_theme_info_missing_engine          (const gchar *ctk_theme,
                                                            gboolean nameOnly);

/* Icon Themes */
CafeThemeIconInfo *cafe_theme_icon_info_new              (void);
void                cafe_theme_icon_info_free             (CafeThemeIconInfo *icon_theme_info);
CafeThemeIconInfo *cafe_theme_icon_info_find             (const gchar        *icon_theme_name);
GList              *cafe_theme_icon_info_find_all         (void);
gint                cafe_theme_icon_info_compare          (CafeThemeIconInfo *a,
							    CafeThemeIconInfo *b);

/* Cursor Themes */
CafeThemeCursorInfo *cafe_theme_cursor_info_new	   (void);
void                  cafe_theme_cursor_info_free	   (CafeThemeCursorInfo *info);
CafeThemeCursorInfo *cafe_theme_cursor_info_find	   (const gchar          *name);
GList                *cafe_theme_cursor_info_find_all	   (void);
gint                  cafe_theme_cursor_info_compare      (CafeThemeCursorInfo *a,
							    CafeThemeCursorInfo *b);

/* Meta themes*/
CafeThemeMetaInfo *cafe_theme_meta_info_new              (void);
void                cafe_theme_meta_info_free             (CafeThemeMetaInfo *meta_theme_info);
CafeThemeMetaInfo *cafe_theme_meta_info_find             (const gchar        *meta_theme_name);
GList              *cafe_theme_meta_info_find_all         (void);
gint                cafe_theme_meta_info_compare          (CafeThemeMetaInfo *a,
							    CafeThemeMetaInfo *b);
gboolean            cafe_theme_meta_info_validate         (const CafeThemeMetaInfo *info,
                                                            GError            **error);
CafeThemeMetaInfo *cafe_theme_read_meta_theme            (GFile              *meta_theme_uri);

/* Other */
void                cafe_theme_init                       (void);
void                cafe_theme_info_register_theme_change (ThemeChangedCallback func,
							    gpointer             data);

gboolean            cafe_theme_color_scheme_parse         (const gchar         *scheme,
							    GdkRGBA             *colors);
gboolean            cafe_theme_color_scheme_equal         (const gchar         *s1,
							    const gchar         *s2);

#endif /* CAFE_THEME_INFO_H */
