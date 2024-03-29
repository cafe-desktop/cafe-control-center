/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
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

#include "config.h"

#include <glib.h>
#include <ctk/ctk.h>
#include <gio/gio.h>
#include <libcafe-desktop/cafe-desktop-thumbnail.h>

#include "cafe-theme-info.h"

#define APPEARANCE_SCHEMA            "org.cafe.control-center.appearance"
#define MORE_THEMES_URL_KEY          "more-themes-url"
#define MORE_BACKGROUNDS_URL_KEY     "more-backgrounds-url"

#define WP_SCHEMA                    "org.cafe.background"
#define WP_FILE_KEY                  "picture-filename"
#define WP_OPTIONS_KEY               "picture-options"
#define WP_SHADING_KEY               "color-shading-type"
#define WP_PCOLOR_KEY                "primary-color"
#define WP_SCOLOR_KEY                "secondary-color"

#define INTERFACE_SCHEMA             "org.cafe.interface"
#define CTK_FONT_KEY                 "font-name"
#define MONOSPACE_FONT_KEY           "monospace-font-name"
#define DOCUMENT_FONT_KEY            "document-font-name"
#define CTK_THEME_KEY                "ctk-theme"
#define ICON_THEME_KEY               "icon-theme"
#define COLOR_SCHEME_KEY             "ctk-color-scheme"
#define ACCEL_CHANGE_KEY             "can-change-accels"
#define MENU_ICONS_KEY               "menus-have-icons"
#define BUTTON_ICONS_KEY             "buttons-have-icons"
#define TOOLBAR_STYLE_KEY            "toolbar-style"
#define CTK_FONT_DEFAULT_VALUE       "Sans 10"

#define LOCKDOWN_SCHEMA              "org.cafe.lockdown"
#define DISABLE_THEMES_SETTINGS_KEY  "disable-theme-settings"

#define BAUL_SCHEMA                  "org.cafe.baul.desktop"
#define DESKTOP_FONT_KEY             "font"

#define CROMA_SCHEMA                 "org.cafe.Croma.general"
#define CROMA_THEME_KEY              "theme"
#define WINDOW_TITLE_FONT_KEY        "titlebar-font"
#define WINDOW_TITLE_USES_SYSTEM_KEY "titlebar-uses-system-font"

#define NOTIFICATION_SCHEMA          "org.cafe.NotificationDaemon"
#define NOTIFICATION_THEME_KEY       "theme"

#define MOUSE_SCHEMA                 "org.cafe.peripherals-mouse"
#define CURSOR_THEME_KEY             "cursor-theme"
#define CURSOR_SIZE_KEY              "cursor-size"

#define FONT_RENDER_SCHEMA           "org.cafe.font-rendering"
#define FONT_ANTIALIASING_KEY        "antialiasing"
#define FONT_HINTING_KEY             "hinting"
#define FONT_RGBA_ORDER_KEY          "rgba-order"
#define FONT_DPI_KEY                 "dpi"

typedef struct {
	GSettings* settings;
	GSettings* wp_settings;
	GSettings* baul_settings;
	GSettings* interface_settings;
	GSettings* croma_settings;
	GSettings* mouse_settings;
	GSettings* font_settings;
	CtkBuilder* ui;
	CafeDesktopThumbnailFactory* thumb_factory;
	gulong screen_size_handler;
	gulong screen_monitors_handler;

	/* desktop */
	GHashTable* wp_hash;
	CtkIconView* wp_view;
	CtkTreeModel* wp_model;
	CtkWidget* wp_scpicker;
	CtkWidget* wp_pcpicker;
	CtkWidget* wp_style_menu;
	CtkWidget* wp_color_menu;
	CtkWidget* wp_rem_button;
	CtkFileChooser* wp_filesel;
	CtkWidget* wp_image;
	GSList* wp_uris;
	gint frame;
	gint thumb_width;
	gint thumb_height;

	/* font */
	CtkWidget* font_details;
	GSList* font_groups;

	/* themes */
	CtkListStore* theme_store;
	CafeThemeMetaInfo* theme_custom;
	GdkPixbuf* theme_icon;
	CtkWidget* theme_save_dialog;
	CtkWidget* theme_message_area;
	CtkWidget* theme_message_label;
	CtkWidget* apply_background_button;
	CtkWidget* revert_font_button;
	CtkWidget* apply_font_button;
	CtkWidget* install_button;
	CtkWidget* theme_info_icon;
	CtkWidget* theme_error_icon;
	gchar* revert_application_font;
	gchar* revert_documents_font;
	gchar* revert_desktop_font;
	gchar* revert_windowtitle_font;
	gchar* revert_monospace_font;

	/* style */
	GdkPixbuf* ctk_theme_icon;
	GdkPixbuf* window_theme_icon;
	GdkPixbuf* icon_theme_icon;
	CtkWidget* style_message_area;
	CtkWidget* style_message_label;
	CtkWidget* style_install_button;
} AppearanceData;

#define appearance_capplet_get_widget(x, y) (CtkWidget*) ctk_builder_get_object(x->ui, y)
