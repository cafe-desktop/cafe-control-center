/*
 * Copyright (C) 2007, 2010 The GNOME Foundation
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
#include "appearance.h"

#include <string.h>
#include <pango/pango.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "theme-util.h"
#include "ctkrc-utils.h"
#include "theme-thumbnail.h"
#include "capplet-util.h"

#define GSETTINGS_SETTINGS "GSETTINGS_SETTINGS"
#define GSETTINGS_KEY      "GSETTINGS_KEY"
#define THEME_DATA         "THEME_DATA"

typedef void (* ThumbnailGenFunc) (void               *type,
				   ThemeThumbnailFunc  theme,
				   AppearanceData     *data,
				   GDestroyNotify     *destroy);

typedef struct {
  AppearanceData *data;
  GdkPixbuf *thumbnail;
} ThemeConvData;

static void update_message_area (AppearanceData *data);
static void create_thumbnail (const gchar *name, GdkPixbuf *default_thumb, AppearanceData *data);

static const gchar *symbolic_names[NUM_SYMBOLIC_COLORS] = {
  "fg_color", "bg_color",
  "text_color", "base_color",
  "selected_fg_color", "selected_bg_color",
  "tooltip_fg_color", "tooltip_bg_color"
};

static gchar *
find_string_in_model (CtkTreeModel *model, const gchar *value, gint column)
{
  CtkTreeIter iter;
  gboolean valid;
  gchar *path = NULL, *test;

  if (!value)
    return NULL;

  for (valid = ctk_tree_model_get_iter_first (model, &iter); valid;
       valid = ctk_tree_model_iter_next (model, &iter))
  {
    ctk_tree_model_get (model, &iter, column, &test, -1);

    if (test)
    {
      gint cmp = strcmp (test, value);
      g_free (test);

      if (!cmp)
      {
        path = ctk_tree_model_get_string_from_iter (model, &iter);
        break;
      }
    }
  }

  return path;
}

static void
treeview_gsettings_changed_callback (GSettings *settings, gchar *key, CtkTreeView *list)
{
  CtkTreeModel *store;
  gchar *curr_value;
  gchar *path;

  /* find value in model */
  curr_value = g_settings_get_string (settings, key);
  store = ctk_tree_view_get_model (list);

  path = find_string_in_model (store, curr_value, COL_NAME);

  /* Add a temporary item if we can't find a match
   * TODO: delete this item if it is no longer selected?
   */
  if (!path)
  {
    CtkListStore *list_store;
    CtkTreeIter iter, sort_iter;
    ThemeConvData *conv;

    list_store = GTK_LIST_STORE (ctk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (store)));

    conv = g_object_get_data (G_OBJECT(list), THEME_DATA);
    ctk_list_store_insert_with_values (list_store, &iter, 0,
                                       COL_LABEL, curr_value,
                                       COL_NAME, curr_value,
                                       COL_THUMBNAIL, conv->thumbnail,
                                       -1);
    /* convert the tree store iter for use with the sort model */
    ctk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (store),
                                                    &sort_iter, &iter);
    path = ctk_tree_model_get_string_from_iter (store, &sort_iter);

    create_thumbnail (curr_value, conv->thumbnail, conv->data);
  }
  /* select the new gsettings theme in treeview */
  CtkTreeSelection *selection = ctk_tree_view_get_selection (list);
  CtkTreePath *treepath = ctk_tree_path_new_from_string (path);
  ctk_tree_selection_select_path (selection, treepath);
  ctk_tree_view_scroll_to_cell (list, treepath, NULL, FALSE, 0, 0);
  ctk_tree_path_free (treepath);
}

static void
treeview_selection_changed_callback (CtkTreeSelection *selection, guint data)
{
  GSettings *settings;
  gchar *key;
  CtkTreeIter iter;
  CtkTreeModel *model;
  CtkTreeView *list;

  list = ctk_tree_selection_get_tree_view (selection);

  settings = g_object_get_data (G_OBJECT (list), GSETTINGS_SETTINGS);
  key = g_object_get_data (G_OBJECT (list), GSETTINGS_KEY);

  if (ctk_tree_selection_get_selected (selection, &model, &iter)) {
    gchar *list_value;

    ctk_tree_model_get (model, &iter, COL_NAME, &list_value, -1);

    if (list_value) {
      g_settings_set_string (settings, key, list_value);
    }
  }
}

static gint
cursor_theme_sort_func (CtkTreeModel *model,
                        CtkTreeIter *a,
                        CtkTreeIter *b,
                        gpointer user_data)
{
  gchar *a_label = NULL;
  gchar *b_label = NULL;
  const gchar *default_label;
  gint result;

  ctk_tree_model_get (model, a, COL_LABEL, &a_label, -1);
  ctk_tree_model_get (model, b, COL_LABEL, &b_label, -1);

  default_label = _("Default Pointer");

  if (!strcmp (a_label, default_label))
    result = -1;
  else if (!strcmp (b_label, default_label))
    result = 1;
  else
    result = strcmp (a_label, b_label);

  g_free (a_label);
  g_free (b_label);

  return result;
}

static void
style_message_area_response_cb (CtkWidget *w,
                                gint response_id,
                                AppearanceData *data)
{
  CtkSettings *settings = ctk_settings_get_default ();
  gchar *theme;
  gchar *engine_path;

  g_object_get (settings, "ctk-theme-name", &theme, NULL);
  engine_path = ctk_theme_info_missing_engine (theme, FALSE);
  g_free (theme);

  if (engine_path != NULL) {
    theme_install_file (GTK_WINDOW (ctk_widget_get_toplevel (data->style_message_area)),
                        engine_path);
    g_free (engine_path);
  }
  update_message_area (data);
}

static void update_message_area(AppearanceData* data)
{
	CtkSettings* settings = ctk_settings_get_default();
	gchar* theme = NULL;
	gchar* engine;

	g_object_get(settings, "ctk-theme-name", &theme, NULL);
	engine = ctk_theme_info_missing_engine(theme, TRUE);
	g_free(theme);

	if (data->style_message_area == NULL)
	{
		CtkWidget* hbox;
		CtkWidget* parent;
		CtkWidget* icon;
		CtkWidget* content;

		if (engine == NULL)
		{
			return;
		}

		data->style_message_area = ctk_info_bar_new ();

		g_signal_connect (data->style_message_area, "response", (GCallback) style_message_area_response_cb, data);

		data->style_install_button = ctk_info_bar_add_button(GTK_INFO_BAR (data->style_message_area), _("Install"), GTK_RESPONSE_APPLY);

		data->style_message_label = ctk_label_new (NULL);
		ctk_label_set_line_wrap (GTK_LABEL (data->style_message_label), TRUE);
		ctk_label_set_xalign (GTK_LABEL (data->style_message_label), 0.0);

		hbox = ctk_box_new (GTK_ORIENTATION_HORIZONTAL, 9);
		icon = ctk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);
		ctk_widget_set_halign (icon, GTK_ALIGN_CENTER);
		ctk_widget_set_valign (icon, GTK_ALIGN_START);
		ctk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
		ctk_box_pack_start (GTK_BOX (hbox), data->style_message_label, TRUE, TRUE, 0);
		content = ctk_info_bar_get_content_area (GTK_INFO_BAR (data->style_message_area));
		ctk_container_add (GTK_CONTAINER (content), hbox);
		ctk_widget_show_all (data->style_message_area);
		ctk_widget_set_no_show_all (data->style_message_area, TRUE);

		parent = appearance_capplet_get_widget (data, "ctk_themes_vbox");
		ctk_box_pack_start (GTK_BOX (parent), data->style_message_area, FALSE, FALSE, 0);
	}

  ctk_widget_hide(data->style_message_area);
}

static void
update_color_buttons_from_string (const gchar *color_scheme, AppearanceData *data)
{
  GdkRGBA colors[NUM_SYMBOLIC_COLORS];
  CtkWidget *widget;
  gint i;

  if (!cafe_theme_color_scheme_parse (color_scheme, colors))
    return;

  /* now set all the buttons to the correct settings */
  for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
    widget = appearance_capplet_get_widget (data, symbolic_names[i]);
    ctk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (widget), &colors[i]);
  }
}

static void
update_color_buttons_from_settings (CtkSettings *settings,
                                    AppearanceData *data)
{
  gchar *scheme, *setting;

  scheme = g_settings_get_string (data->interface_settings, COLOR_SCHEME_KEY);
  g_object_get (settings, "ctk-color-scheme", &setting, NULL);

  if (scheme == NULL || strcmp (scheme, "") == 0)
    ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "color_scheme_defaults_button"), FALSE);

  g_free (scheme);
  update_color_buttons_from_string (setting, data);
  g_free (setting);
}

static void
color_scheme_changed (GObject    *settings,
                      GParamSpec *pspec,
                      AppearanceData  *data)
{
  update_color_buttons_from_settings (GTK_SETTINGS (settings), data);
}

static void
check_color_schemes_enabled (CtkSettings *settings,
                             AppearanceData *data)
{
  gchar *theme = NULL;
  gchar *filename;
  GSList *symbolic_colors = NULL;
  gboolean enable_colors = FALSE;
  gint i;

  g_object_get (settings, "ctk-theme-name", &theme, NULL);
  filename = ctkrc_find_named (theme);
  g_free (theme);

  ctkrc_get_details (filename, NULL, &symbolic_colors);
  g_free (filename);

  for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
    gboolean found;

    found = (g_slist_find_custom (symbolic_colors, symbolic_names[i], (GCompareFunc) strcmp) != NULL);
    ctk_widget_set_sensitive (appearance_capplet_get_widget (data, symbolic_names[i]), found);

    enable_colors |= found;
  }

  g_slist_foreach (symbolic_colors, (GFunc) g_free, NULL);
  g_slist_free (symbolic_colors);

  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "color_scheme_table"), enable_colors);
  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "color_scheme_defaults_button"), enable_colors);

  if (enable_colors)
    ctk_widget_hide (appearance_capplet_get_widget (data, "color_scheme_message_hbox"));
  else
    ctk_widget_show (appearance_capplet_get_widget (data, "color_scheme_message_hbox"));
}

static void
color_button_clicked_cb (CtkWidget *colorbutton, AppearanceData *data)
{
  CtkWidget *widget;
  GdkRGBA color;
  GString *scheme = g_string_new (NULL);
  gchar *colstr;
  gchar *old_scheme = NULL;
  gint i;

  for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
    widget = appearance_capplet_get_widget (data, symbolic_names[i]);
    ctk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (widget), &color);

    colstr = gdk_rgba_to_string (&color);
    g_string_append_printf (scheme, "%s:%s\n", symbolic_names[i], colstr);
    g_free (colstr);
  }
  /* remove the last newline */
  g_string_truncate (scheme, scheme->len - 1);

  /* verify that the scheme really has changed */
  g_object_get (ctk_settings_get_default (), "ctk-color-scheme", &old_scheme, NULL);

  if (!cafe_theme_color_scheme_equal (old_scheme, scheme->str)) {
    g_settings_set_string (data->interface_settings, COLOR_SCHEME_KEY, scheme->str);

    ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "color_scheme_defaults_button"), TRUE);
  }
  g_free (old_scheme);
  g_string_free (scheme, TRUE);
}

static void
color_scheme_defaults_button_clicked_cb (CtkWidget *button, AppearanceData *data)
{
  g_settings_reset (data->interface_settings, COLOR_SCHEME_KEY);
  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "color_scheme_defaults_button"), FALSE);
}

static void
style_response_cb (CtkDialog *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_HELP) {
    capplet_help (GTK_WINDOW (dialog), "goscustdesk-61");
  } else {
    ctk_widget_hide (GTK_WIDGET (dialog));
  }
}

static void
ctk_theme_changed (GSettings *settings, gchar *key, AppearanceData *data)
{
  CafeThemeInfo *theme = NULL;
  gchar *name;
  CtkSettings *ctksettings = ctk_settings_get_default ();

  name = g_settings_get_string (settings, key);
  if (name) {
    gchar *current;

    theme = cafe_theme_info_find (name);

    /* Manually update CtkSettings to new ctk+ theme.
     * This will eventually happen anyway, but we need the
     * info for the color scheme updates already. */
    g_object_get (ctksettings, "ctk-theme-name", &current, NULL);

    if (strcmp (current, name) != 0) {
      g_object_set (ctksettings, "ctk-theme-name", name, NULL);
      update_message_area (data);
    }

    g_free (current);

    check_color_schemes_enabled (ctksettings, data);
    update_color_buttons_from_settings (ctksettings, data);
    g_free (name);
  }

  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "ctk_themes_delete"),
			    theme_is_writable (theme));
}

static void
window_theme_changed (GSettings *settings, gchar *key, AppearanceData *data)
{
  CafeThemeInfo *theme = NULL;
  gchar *name;

  name = g_settings_get_string (settings, key);
  if (name)
  {
    theme = cafe_theme_info_find (name);
    g_free (name);
  }

  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "window_themes_delete"),
			    theme_is_writable (theme));
}

static void
icon_theme_changed (GSettings *settings, gchar *key, AppearanceData *data)
{
  CafeThemeIconInfo *theme = NULL;
  gchar *name;

  name = g_settings_get_string (settings, key);
  if (name) {
    theme = cafe_theme_icon_info_find (name);
    g_free (name);
}

  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "icon_themes_delete"),
			    theme_is_writable (theme));
}

static void
cursor_size_changed_cb (int size, AppearanceData *data)
{
  g_settings_set_int (data->mouse_settings, CURSOR_SIZE_KEY, size);
}

static void
cursor_size_scale_value_changed_cb (CtkRange *range, AppearanceData *data)
{
  CafeThemeCursorInfo *theme;
  gchar *name;

  name = g_settings_get_string (data->mouse_settings, CURSOR_THEME_KEY);
  if (name == NULL)
    return;

  theme = cafe_theme_cursor_info_find (name);
  g_free (name);

  if (theme) {
    gint size;

    size = g_array_index (theme->sizes, gint, (int) ctk_range_get_value (range));
    cursor_size_changed_cb (size, data);
  }
}

static void
update_cursor_size_scale (CafeThemeCursorInfo *theme,
                          AppearanceData *data)
{
  CtkWidget *cursor_size_scale;
  CtkWidget *cursor_size_label;
  CtkWidget *cursor_size_small_label;
  CtkWidget *cursor_size_large_label;
  gboolean sensitive;
  gint size, gsettings_size;

  cursor_size_scale = appearance_capplet_get_widget (data, "cursor_size_scale");
  cursor_size_label = appearance_capplet_get_widget (data, "cursor_size_label");
  cursor_size_small_label = appearance_capplet_get_widget (data, "cursor_size_small_label");
  cursor_size_large_label = appearance_capplet_get_widget (data, "cursor_size_large_label");

  sensitive = theme && theme->sizes->len > 1;
  ctk_widget_set_sensitive (cursor_size_scale, sensitive);
  ctk_widget_set_sensitive (cursor_size_label, sensitive);
  ctk_widget_set_sensitive (cursor_size_small_label, sensitive);
  ctk_widget_set_sensitive (cursor_size_large_label, sensitive);

  gsettings_size = g_settings_get_int (data->mouse_settings, CURSOR_SIZE_KEY);

  if (sensitive) {
    CtkAdjustment *adjustment;
    gint i, index;
    CtkRange *range = GTK_RANGE (cursor_size_scale);

    adjustment = ctk_range_get_adjustment (range);
    g_object_set (adjustment, "upper", (gdouble) theme->sizes->len - 1, NULL);


    /* fallback if the gsettings value is bigger than all available sizes;
       use the largest we have */
    index = theme->sizes->len - 1;

    /* set the slider to the cursor size which matches the gsettings setting best  */
    for (i = 0; i < theme->sizes->len; i++) {
      size = g_array_index (theme->sizes, gint, i);

      if (size == gsettings_size) {
      	index = i;
        break;
      } else if (size > gsettings_size) {
        if (i == 0) {
          index = 0;
        } else {
          gint diff, diff_to_last;

          diff = size - gsettings_size;
          diff_to_last = gsettings_size - g_array_index (theme->sizes, gint, i - 1);

          index = (diff < diff_to_last) ? i : i - 1;
        }
        break;
      }
    }

    ctk_range_set_value (range, (gdouble) index);

    size = g_array_index (theme->sizes, gint, index);
  } else {
    if (theme && theme->sizes->len > 0)
      size = g_array_index (theme->sizes, gint, 0);
    else
      size = 24;
  }

  if (size != gsettings_size)
    cursor_size_changed_cb (size, data);
}

static void
cursor_theme_changed (GSettings *settings, gchar *key, AppearanceData *data)
{
  CafeThemeCursorInfo *theme = NULL;
  gchar *name;

  name = g_settings_get_string (settings, key);
  if (name) {
    theme = cafe_theme_cursor_info_find (name);
    g_free (name);
  }

  update_cursor_size_scale (theme, data);

  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "cursor_themes_delete"),
			    theme_is_writable (theme));

}

static void
generic_theme_delete (const gchar *tv_name, ThemeType type, AppearanceData *data)
{
  CtkTreeView *treeview = GTK_TREE_VIEW (appearance_capplet_get_widget (data, tv_name));
  CtkTreeSelection *selection = ctk_tree_view_get_selection (treeview);
  CtkTreeModel *model;
  CtkTreeIter iter;

  if (ctk_tree_selection_get_selected (selection, &model, &iter)) {
    gchar *name;

    ctk_tree_model_get (model, &iter, COL_NAME, &name, -1);

    if (name != NULL && theme_delete (name, type)) {
      /* remove theme from the model, too */
      CtkTreeIter child;
      CtkTreePath *path;

      path = ctk_tree_model_get_path (model, &iter);
      ctk_tree_model_sort_convert_iter_to_child_iter (
          GTK_TREE_MODEL_SORT (model), &child, &iter);
      ctk_list_store_remove (GTK_LIST_STORE (
          ctk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model))), &child);

      if (ctk_tree_model_get_iter (model, &iter, path) ||
          theme_model_iter_last (model, &iter)) {
        ctk_tree_path_free (path);
        path = ctk_tree_model_get_path (model, &iter);
	ctk_tree_selection_select_path (selection, path);
	ctk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0, 0);
      }
      ctk_tree_path_free (path);
    }
    g_free (name);
  }
}

static void
ctk_theme_delete_cb (CtkWidget *button, AppearanceData *data)
{
  generic_theme_delete ("ctk_themes_list", THEME_TYPE_GTK, data);
}

static void
window_theme_delete_cb (CtkWidget *button, AppearanceData *data)
{
  generic_theme_delete ("window_themes_list", THEME_TYPE_WINDOW, data);
}

static void
icon_theme_delete_cb (CtkWidget *button, AppearanceData *data)
{
  generic_theme_delete ("icon_themes_list", THEME_TYPE_ICON, data);
}

static void
cursor_theme_delete_cb (CtkWidget *button, AppearanceData *data)
{
  generic_theme_delete ("cursor_themes_list", THEME_TYPE_CURSOR, data);
}

static void
add_to_treeview (const gchar *tv_name,
		 const gchar *theme_name,
		 const gchar *theme_label,
		 GdkPixbuf *theme_thumbnail,
		 AppearanceData *data)
{
  CtkTreeView *treeview;
  CtkListStore *model;

  treeview = GTK_TREE_VIEW (appearance_capplet_get_widget (data, tv_name));
  model = GTK_LIST_STORE (
          ctk_tree_model_sort_get_model (
          GTK_TREE_MODEL_SORT (ctk_tree_view_get_model (treeview))));

  ctk_list_store_insert_with_values (model, NULL, 0,
          COL_LABEL, theme_label,
          COL_NAME, theme_name,
          COL_THUMBNAIL, theme_thumbnail,
          -1);
}

static void
remove_from_treeview (const gchar *tv_name,
		      const gchar *theme_name,
		      AppearanceData *data)
{
  CtkTreeView *treeview;
  CtkListStore *model;
  CtkTreeIter iter;

  treeview = GTK_TREE_VIEW (appearance_capplet_get_widget (data, tv_name));
  model = GTK_LIST_STORE (
          ctk_tree_model_sort_get_model (
          GTK_TREE_MODEL_SORT (ctk_tree_view_get_model (treeview))));

  if (theme_find_in_model (GTK_TREE_MODEL (model), theme_name, &iter))
    ctk_list_store_remove (model, &iter);
}

static void
update_in_treeview (const gchar *tv_name,
		    const gchar *theme_name,
		    const gchar *theme_label,
		    AppearanceData *data)
{
  CtkTreeView *treeview;
  CtkListStore *model;
  CtkTreeIter iter;

  treeview = GTK_TREE_VIEW (appearance_capplet_get_widget (data, tv_name));
  model = GTK_LIST_STORE (
          ctk_tree_model_sort_get_model (
          GTK_TREE_MODEL_SORT (ctk_tree_view_get_model (treeview))));

  if (theme_find_in_model (GTK_TREE_MODEL (model), theme_name, &iter)) {
    ctk_list_store_set (model, &iter,
          COL_LABEL, theme_label,
          COL_NAME, theme_name,
          -1);
  }
}

static void
update_thumbnail_in_treeview (const gchar *tv_name,
		    const gchar *theme_name,
		    GdkPixbuf *theme_thumbnail,
		    AppearanceData *data)
{
  CtkTreeView *treeview;
  CtkListStore *model;
  CtkTreeIter iter;

  if (theme_thumbnail == NULL)
    return;

  treeview = GTK_TREE_VIEW (appearance_capplet_get_widget (data, tv_name));
  model = GTK_LIST_STORE (
          ctk_tree_model_sort_get_model (
          GTK_TREE_MODEL_SORT (ctk_tree_view_get_model (treeview))));

  if (theme_find_in_model (GTK_TREE_MODEL (model), theme_name, &iter)) {
    ctk_list_store_set (model, &iter,
          COL_THUMBNAIL, theme_thumbnail,
          -1);
  }
}

static void
ctk_theme_thumbnail_cb (GdkPixbuf *pixbuf,
                        gchar *theme_name,
                        AppearanceData *data)
{
  update_thumbnail_in_treeview ("ctk_themes_list", theme_name, pixbuf, data);
}

static void
croma_theme_thumbnail_cb (GdkPixbuf *pixbuf,
                             gchar *theme_name,
                             AppearanceData *data)
{
  update_thumbnail_in_treeview ("window_themes_list", theme_name, pixbuf, data);
}

static void
icon_theme_thumbnail_cb (GdkPixbuf *pixbuf,
                         gchar *theme_name,
                         AppearanceData *data)
{
  update_thumbnail_in_treeview ("icon_themes_list", theme_name, pixbuf, data);
}

static void
create_thumbnail (const gchar *name, GdkPixbuf *default_thumb, AppearanceData *data)
{
  if (default_thumb == data->icon_theme_icon) {
    CafeThemeIconInfo *info;
    info = cafe_theme_icon_info_find (name);
    if (info != NULL) {
      generate_icon_theme_thumbnail_async (info,
          (ThemeThumbnailFunc) icon_theme_thumbnail_cb, data, NULL);
    }
  } else if (default_thumb == data->ctk_theme_icon) {
    CafeThemeInfo *info;
    info = cafe_theme_info_find (name);
    if (info != NULL && info->has_ctk) {
      generate_ctk_theme_thumbnail_async (info,
          (ThemeThumbnailFunc) ctk_theme_thumbnail_cb, data, NULL);
    }
  } else if (default_thumb == data->window_theme_icon) {
    CafeThemeInfo *info;
    info = cafe_theme_info_find (name);
    if (info != NULL && info->has_croma) {
      generate_croma_theme_thumbnail_async (info,
          (ThemeThumbnailFunc) croma_theme_thumbnail_cb, data, NULL);
    }
  }
}

static void
changed_on_disk_cb (CafeThemeCommonInfo *theme,
		    CafeThemeChangeType  change_type,
                    CafeThemeElement     element_type,
		    AppearanceData       *data)
{
  if (theme->type == CAFE_THEME_TYPE_REGULAR) {
    CafeThemeInfo *info = (CafeThemeInfo *) theme;

    if (change_type == CAFE_THEME_CHANGE_DELETED) {
      if (element_type & CAFE_THEME_GTK_2)
        remove_from_treeview ("ctk_themes_list", info->name, data);
      if (element_type & CAFE_THEME_CROMA)
        remove_from_treeview ("window_themes_list", info->name, data);

    } else {
      if (element_type & CAFE_THEME_GTK_2) {
        if (change_type == CAFE_THEME_CHANGE_CREATED)
          add_to_treeview ("ctk_themes_list", info->name, info->name, data->ctk_theme_icon, data);
        else if (change_type == CAFE_THEME_CHANGE_CHANGED)
          update_in_treeview ("ctk_themes_list", info->name, info->name, data);

        generate_ctk_theme_thumbnail_async (info,
            (ThemeThumbnailFunc) ctk_theme_thumbnail_cb, data, NULL);
      }

      if (element_type & CAFE_THEME_CROMA) {
        if (change_type == CAFE_THEME_CHANGE_CREATED)
          add_to_treeview ("window_themes_list", info->name, info->name, data->window_theme_icon, data);
        else if (change_type == CAFE_THEME_CHANGE_CHANGED)
          update_in_treeview ("window_themes_list", info->name, info->name, data);

        generate_croma_theme_thumbnail_async (info,
            (ThemeThumbnailFunc) croma_theme_thumbnail_cb, data, NULL);
      }
    }

  } else if (theme->type == CAFE_THEME_TYPE_ICON) {
    CafeThemeIconInfo *info = (CafeThemeIconInfo *) theme;

    if (change_type == CAFE_THEME_CHANGE_DELETED) {
      remove_from_treeview ("icon_themes_list", info->name, data);
    } else {
      if (change_type == CAFE_THEME_CHANGE_CREATED)
        add_to_treeview ("icon_themes_list", info->name, info->readable_name, data->icon_theme_icon, data);
      else if (change_type == CAFE_THEME_CHANGE_CHANGED)
        update_in_treeview ("icon_themes_list", info->name, info->readable_name, data);

      generate_icon_theme_thumbnail_async (info,
          (ThemeThumbnailFunc) icon_theme_thumbnail_cb, data, NULL);
    }

  } else if (theme->type == CAFE_THEME_TYPE_CURSOR) {
    CafeThemeCursorInfo *info = (CafeThemeCursorInfo *) theme;

    if (change_type == CAFE_THEME_CHANGE_DELETED) {
      remove_from_treeview ("cursor_themes_list", info->name, data);
    } else {
      if (change_type == CAFE_THEME_CHANGE_CREATED)
        add_to_treeview ("cursor_themes_list", info->name, info->readable_name, info->thumbnail, data);
      else if (change_type == CAFE_THEME_CHANGE_CHANGED)
        update_in_treeview ("cursor_themes_list", info->name, info->readable_name, data);
    }
  }
}

static void
prepare_list (AppearanceData *data, CtkWidget *list, ThemeType type, GCallback callback)
{
  CtkListStore *store;
  GList *l, *themes = NULL;
  CtkCellRenderer *renderer;
  CtkTreeViewColumn *column;
  CtkTreeModel *sort_model;
  GdkPixbuf *thumbnail;
  const gchar *key;
  ThumbnailGenFunc generator;
  ThemeThumbnailFunc thumb_cb;
  ThemeConvData *conv_data;
  GSettings *settings;

  switch (type)
  {
    case THEME_TYPE_GTK:
      themes = cafe_theme_info_find_by_type (CAFE_THEME_GTK_2);
      thumbnail = data->ctk_theme_icon;
      settings = data->interface_settings;
      key = GTK_THEME_KEY;
      generator = (ThumbnailGenFunc) generate_ctk_theme_thumbnail_async;
      thumb_cb = (ThemeThumbnailFunc) ctk_theme_thumbnail_cb;
      break;

    case THEME_TYPE_WINDOW:
      themes = cafe_theme_info_find_by_type (CAFE_THEME_CROMA);
      thumbnail = data->window_theme_icon;
      settings = data->croma_settings;
      key = CROMA_THEME_KEY;
      generator = (ThumbnailGenFunc) generate_croma_theme_thumbnail_async;
      thumb_cb = (ThemeThumbnailFunc) croma_theme_thumbnail_cb;
      break;

    case THEME_TYPE_ICON:
      themes = cafe_theme_icon_info_find_all ();
      thumbnail = data->icon_theme_icon;
      settings = data->interface_settings;
      key = ICON_THEME_KEY;
      generator = (ThumbnailGenFunc) generate_icon_theme_thumbnail_async;
      thumb_cb = (ThemeThumbnailFunc) icon_theme_thumbnail_cb;
      break;

    case THEME_TYPE_CURSOR:
      themes = cafe_theme_cursor_info_find_all ();
      thumbnail = NULL;
      settings = data->mouse_settings;
      key = CURSOR_THEME_KEY;
      generator = NULL;
      thumb_cb = NULL;
      break;

    default:
      /* we don't deal with any other type of themes here */
      return;
  }

  store = ctk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

  for (l = themes; l; l = g_list_next (l))
  {
    CafeThemeCommonInfo *theme = (CafeThemeCommonInfo *) l->data;
    CtkTreeIter i;

    if (type == THEME_TYPE_CURSOR) {
      thumbnail = ((CafeThemeCursorInfo *) theme)->thumbnail;
    } else {
      generator (theme, thumb_cb, data, NULL);
    }

    ctk_list_store_insert_with_values (store, &i, 0,
                                       COL_LABEL, theme->readable_name,
                                       COL_NAME, theme->name,
                                       COL_THUMBNAIL, thumbnail,
                                       -1);

    if (type == THEME_TYPE_CURSOR && thumbnail) {
      g_object_unref (thumbnail);
      thumbnail = NULL;
    }
  }
  g_list_free (themes);

  sort_model = ctk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  ctk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        COL_LABEL, GTK_SORT_ASCENDING);

  if (type == THEME_TYPE_CURSOR)
    ctk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model), COL_LABEL,
                                     (CtkTreeIterCompareFunc) cursor_theme_sort_func,
                                     NULL, NULL);

  ctk_tree_view_set_model (GTK_TREE_VIEW (list), GTK_TREE_MODEL (sort_model));

  renderer = ctk_cell_renderer_pixbuf_new ();
  g_object_set (renderer, "xpad", 3, "ypad", 3, NULL);

  column = ctk_tree_view_column_new ();
  ctk_tree_view_column_pack_start (column, renderer, FALSE);
  ctk_tree_view_column_add_attribute (column, renderer, "pixbuf", COL_THUMBNAIL);
  ctk_tree_view_append_column (GTK_TREE_VIEW (list), column);

  renderer = ctk_cell_renderer_text_new ();

  column = ctk_tree_view_column_new ();
  ctk_tree_view_column_pack_start (column, renderer, FALSE);
  ctk_tree_view_column_add_attribute (column, renderer, "text", COL_LABEL);
  ctk_tree_view_append_column (GTK_TREE_VIEW (list), column);

  conv_data = g_new (ThemeConvData, 1);
  conv_data->data = data;
  conv_data->thumbnail = thumbnail;

  /* set useful data for callbacks */
  g_object_set_data_full (G_OBJECT (list), THEME_DATA, conv_data, g_free);
  g_object_set_data (G_OBJECT (list), GSETTINGS_SETTINGS, settings);
  g_object_set_data_full (G_OBJECT (list), GSETTINGS_KEY, g_strdup(key), g_free);

  /* select in treeview the theme set in gsettings */
  CtkTreeModel *treemodel;
  treemodel = ctk_tree_view_get_model (GTK_TREE_VIEW (list));
  gchar *theme = g_settings_get_string (settings, key);
  gchar *path = find_string_in_model (treemodel, theme, COL_NAME);
  if (path)
  {
    CtkTreeSelection *selection = ctk_tree_view_get_selection (GTK_TREE_VIEW (list));
    CtkTreePath *treepath = ctk_tree_path_new_from_string (path);
    ctk_tree_selection_select_path (selection, treepath);
    ctk_tree_view_scroll_to_cell (GTK_TREE_VIEW (list), treepath, NULL, FALSE, 0, 0);
    ctk_tree_path_free (treepath);
    g_free (path);
  }
  if (theme)
    g_free (theme);

  /* connect to gsettings change event */
  gchar *signal_name = g_strdup_printf("changed::%s", key);
  g_signal_connect (settings, signal_name,
      G_CALLBACK (treeview_gsettings_changed_callback), list);
  g_signal_connect (settings, signal_name, callback, data);
  g_free (signal_name);

  /* connect to treeview change event */
  g_signal_connect (ctk_tree_view_get_selection (GTK_TREE_VIEW (list)),
      "changed", G_CALLBACK (treeview_selection_changed_callback), list);
}

void
style_init (AppearanceData *data)
{
  CtkSettings *settings;
  CtkWidget *w;
  gchar *label;
  gint i;

  data->ctk_theme_icon = gdk_pixbuf_new_from_file (CAFECC_PIXMAP_DIR "/ctk-theme-thumbnailing.png", NULL);
  data->window_theme_icon = gdk_pixbuf_new_from_file (CAFECC_PIXMAP_DIR "/window-theme-thumbnailing.png", NULL);
  data->icon_theme_icon = gdk_pixbuf_new_from_file (CAFECC_PIXMAP_DIR "/icon-theme-thumbnailing.png", NULL);
  data->style_message_area = NULL;
  data->style_message_label = NULL;
  data->style_install_button = NULL;

  w = appearance_capplet_get_widget (data, "theme_details");
  g_signal_connect (w, "response", (GCallback) style_response_cb, NULL);
  g_signal_connect (w, "delete_event", (GCallback) ctk_true, NULL);

  prepare_list (data, appearance_capplet_get_widget (data, "window_themes_list"), THEME_TYPE_WINDOW, (GCallback) window_theme_changed);
  prepare_list (data, appearance_capplet_get_widget (data, "ctk_themes_list"), THEME_TYPE_GTK, (GCallback) ctk_theme_changed);
  prepare_list (data, appearance_capplet_get_widget (data, "icon_themes_list"), THEME_TYPE_ICON, (GCallback) icon_theme_changed);
  prepare_list (data, appearance_capplet_get_widget (data, "cursor_themes_list"), THEME_TYPE_CURSOR, (GCallback) cursor_theme_changed);

  window_theme_changed (data->croma_settings, CROMA_THEME_KEY, data);
  ctk_theme_changed (data->interface_settings, GTK_THEME_KEY, data);
  icon_theme_changed (data->interface_settings, ICON_THEME_KEY, data);
  cursor_theme_changed (data->mouse_settings, CURSOR_THEME_KEY, data);

  CtkNotebook *style_nb = GTK_NOTEBOOK (appearance_capplet_get_widget (data, "notebook2"));
  ctk_notebook_remove_page (style_nb, 1);

  w = appearance_capplet_get_widget (data, "color_scheme_message_hbox");
  ctk_widget_set_no_show_all (w, TRUE);

  w = appearance_capplet_get_widget (data, "color_scheme_defaults_button");
  ctk_button_set_image (GTK_BUTTON (w),
                        ctk_image_new_from_icon_name ("document-revert",
                                                      GTK_ICON_SIZE_BUTTON));

  settings = ctk_settings_get_default ();
  g_signal_connect (settings, "notify::ctk-color-scheme", (GCallback) color_scheme_changed, data);

  w = appearance_capplet_get_widget (data, "cursor_size_scale");
  g_signal_connect (w, "value-changed", (GCallback) cursor_size_scale_value_changed_cb, data);

  w = appearance_capplet_get_widget (data, "cursor_size_small_label");
  label = g_strdup_printf ("<small><i>%s</i></small>", ctk_label_get_text (GTK_LABEL (w)));
  ctk_label_set_markup (GTK_LABEL (w), label);
  g_free (label);

  w = appearance_capplet_get_widget (data, "cursor_size_large_label");
  label = g_strdup_printf ("<small><i>%s</i></small>", ctk_label_get_text (GTK_LABEL (w)));
  ctk_label_set_markup (GTK_LABEL (w), label);
  g_free (label);

  /* connect signals */
  /* color buttons */
  for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i)
    g_signal_connect (appearance_capplet_get_widget (data, symbolic_names[i]), "color-set", (GCallback) color_button_clicked_cb, data);

  /* revert button */
  g_signal_connect (appearance_capplet_get_widget (data, "color_scheme_defaults_button"), "clicked", (GCallback) color_scheme_defaults_button_clicked_cb, data);
  /* delete buttons */
  g_signal_connect (appearance_capplet_get_widget (data, "ctk_themes_delete"), "clicked", (GCallback) ctk_theme_delete_cb, data);
  g_signal_connect (appearance_capplet_get_widget (data, "window_themes_delete"), "clicked", (GCallback) window_theme_delete_cb, data);
  g_signal_connect (appearance_capplet_get_widget (data, "icon_themes_delete"), "clicked", (GCallback) icon_theme_delete_cb, data);
  g_signal_connect (appearance_capplet_get_widget (data, "cursor_themes_delete"), "clicked", (GCallback) cursor_theme_delete_cb, data);

  update_message_area (data);
  cafe_theme_info_register_theme_change ((ThemeChangedCallback) changed_on_disk_cb, data);
}

void
style_shutdown (AppearanceData *data)
{
  if (data->ctk_theme_icon)
    g_object_unref (data->ctk_theme_icon);
  if (data->window_theme_icon)
    g_object_unref (data->window_theme_icon);
  if (data->icon_theme_icon)
    g_object_unref (data->icon_theme_icon);
}
