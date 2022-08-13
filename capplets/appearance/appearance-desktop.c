/*
 * Copyright (C) 2007,2008 The GNOME Foundation
 * Written by Rodney Dawes <dobey@ximian.com>
 *            Denis Washington <denisw@svn.gnome.org>
 *            Thomas Wood <thos@gnome.org>
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
#include "cafe-wp-info.h"
#include "cafe-wp-item.h"
#include "cafe-wp-xml.h"
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>
#include <libcafe-desktop/cafe-desktop-thumbnail.h>
#include <libcafe-desktop/cafe-bg.h>

enum {
	TARGET_URI_LIST,
	TARGET_BGIMAGE
};

static const CtkTargetEntry drop_types[] = {
	{"text/uri-list", 0, TARGET_URI_LIST},
	{"property/bgimage", 0, TARGET_BGIMAGE}
};

static const CtkTargetEntry drag_types[] = {
	{"text/uri-list", CTK_TARGET_OTHER_WIDGET, TARGET_URI_LIST}
};


static void wp_update_preview(CtkFileChooser* chooser, AppearanceData* data);

static void select_item(AppearanceData* data, CafeWPItem* item, gboolean scroll)
{
	CtkTreePath* path;

	g_return_if_fail(data != NULL);

	if (item == NULL)
		return;

	path = ctk_tree_row_reference_get_path(item->rowref);

	ctk_icon_view_select_path(data->wp_view, path);

	if (scroll)
	{
		ctk_icon_view_scroll_to_path(data->wp_view, path, FALSE, 0.5, 0.0);
	}

	ctk_tree_path_free(path);
}

static CafeWPItem* get_selected_item(AppearanceData* data, CtkTreeIter* iter)
{
	CafeWPItem* item = NULL;
	GList* selected;

	selected = ctk_icon_view_get_selected_items (data->wp_view);

	if (selected != NULL)
	{
		CtkTreeIter sel_iter;

		ctk_tree_model_get_iter(data->wp_model, &sel_iter, selected->data);

		g_list_foreach(selected, (GFunc) ctk_tree_path_free, NULL);
		g_list_free(selected);

		if (iter)
			*iter = sel_iter;

		ctk_tree_model_get(data->wp_model, &sel_iter, 1, &item, -1);
	}

	return item;
}

static gboolean predicate (gpointer key, gpointer value, gpointer data)
{
  CafeBG *bg = data;
  CafeWPItem *item = value;

  return item->bg == bg;
}

static void on_item_changed (CafeBG *bg, AppearanceData *data) {
  CtkTreeModel *model;
  CtkTreeIter iter;
  CtkTreePath *path;
  CafeWPItem *item;

  item = g_hash_table_find (data->wp_hash, predicate, bg);

  if (!item)
    return;

  model = ctk_tree_row_reference_get_model (item->rowref);
  path = ctk_tree_row_reference_get_path (item->rowref);

  if (ctk_tree_model_get_iter (model, &iter, path)) {
    GdkPixbuf *pixbuf;

    g_signal_handlers_block_by_func (bg, G_CALLBACK (on_item_changed), data);

    pixbuf = cafe_wp_item_get_thumbnail (item,
                                          data->thumb_factory,
                                          data->thumb_width,
                                          data->thumb_height);
    if (pixbuf) {
      ctk_list_store_set (CTK_LIST_STORE (data->wp_model), &iter, 0, pixbuf, -1);
      g_object_unref (pixbuf);
    }

    g_signal_handlers_unblock_by_func (bg, G_CALLBACK (on_item_changed), data);
  }
}

static void
wp_props_load_wallpaper (gchar *key,
                         CafeWPItem *item,
                         AppearanceData *data)
{
  CtkTreeIter iter;
  CtkTreePath *path;
  GdkPixbuf *pixbuf;

  if (item->deleted == TRUE)
    return;

  ctk_list_store_append (CTK_LIST_STORE (data->wp_model), &iter);

  pixbuf = cafe_wp_item_get_thumbnail (item, data->thumb_factory,
                                        data->thumb_width,
                                        data->thumb_height);
  cafe_wp_item_update_description (item);

  ctk_list_store_set (CTK_LIST_STORE (data->wp_model), &iter,
                      0, pixbuf,
                      1, item,
                      -1);

  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  path = ctk_tree_model_get_path (data->wp_model, &iter);
  item->rowref = ctk_tree_row_reference_new (data->wp_model, path);
  g_signal_connect (item->bg, "changed", G_CALLBACK (on_item_changed), data);
  ctk_tree_path_free (path);
}

static CafeWPItem *
wp_add_image (AppearanceData *data,
              const gchar *filename)
{
  CafeWPItem *item;

  if (!filename)
    return NULL;

  item = g_hash_table_lookup (data->wp_hash, filename);

  if (item != NULL)
  {
    if (item->deleted)
    {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, data);
    }
  }
  else
  {
    item = cafe_wp_item_new (filename, data->wp_hash, data->thumb_factory);

    if (item != NULL)
    {
      wp_props_load_wallpaper (item->filename, item, data);
    }
  }

  return item;
}

static void
wp_add_images (AppearanceData *data,
               GSList *images)
{
  CdkWindow *window;
  CtkWidget *w;
  CdkCursor *cursor;
  CafeWPItem *item;

  w = appearance_capplet_get_widget (data, "appearance_window");
  window = ctk_widget_get_window (w);

  item = NULL;
  cursor = cdk_cursor_new_for_display (cdk_display_get_default (),
                                       CDK_WATCH);
  cdk_window_set_cursor (window, cursor);
  g_object_unref (cursor);

  while (images != NULL)
  {
    gchar *uri = images->data;

    item = wp_add_image (data, uri);
    images = g_slist_remove (images, uri);
    g_free (uri);
  }

  cdk_window_set_cursor (window, NULL);

  if (item != NULL)
  {
    select_item (data, item, TRUE);
  }
}

static void
wp_option_menu_set (AppearanceData *data,
                    int value,
                    gboolean shade_type)
{
  if (shade_type)
  {
    ctk_combo_box_set_active (CTK_COMBO_BOX (data->wp_color_menu),
                              value);

    if (value == CAFE_BG_COLOR_SOLID)
      ctk_widget_hide (data->wp_scpicker);
    else
      ctk_widget_show (data->wp_scpicker);
  }
  else
  {
    ctk_combo_box_set_active (CTK_COMBO_BOX (data->wp_style_menu),
                              value);
  }
}

static void
wp_set_sensitivities (AppearanceData *data)
{
  CafeWPItem *item;
  gchar *filename = NULL;

  item = get_selected_item (data, NULL);

  if (item != NULL)
    filename = item->filename;

  if (!g_settings_is_writable (data->wp_settings, WP_OPTIONS_KEY)
      || (filename && !strcmp (filename, "(none)")))
    ctk_widget_set_sensitive (data->wp_style_menu, FALSE);
  else
    ctk_widget_set_sensitive (data->wp_style_menu, TRUE);

  if (!g_settings_is_writable (data->wp_settings, WP_SHADING_KEY))
    ctk_widget_set_sensitive (data->wp_color_menu, FALSE);
  else
    ctk_widget_set_sensitive (data->wp_color_menu, TRUE);

  if (!g_settings_is_writable (data->wp_settings, WP_PCOLOR_KEY))
    ctk_widget_set_sensitive (data->wp_pcpicker, FALSE);
  else
    ctk_widget_set_sensitive (data->wp_pcpicker, TRUE);

  if (!g_settings_is_writable (data->wp_settings, WP_SCOLOR_KEY))
    ctk_widget_set_sensitive (data->wp_scpicker, FALSE);
  else
    ctk_widget_set_sensitive (data->wp_scpicker, TRUE);

  if (!filename || !strcmp (filename, "(none)"))
    ctk_widget_set_sensitive (data->wp_rem_button, FALSE);
  else
    ctk_widget_set_sensitive (data->wp_rem_button, TRUE);
}

static void
wp_scale_type_changed (CtkComboBox *combobox,
                       AppearanceData *data)
{
  CafeWPItem *item;
  CtkTreeIter iter;
  GdkPixbuf *pixbuf;

  item = get_selected_item (data, &iter);

  if (item == NULL)
    return;

  item->options = ctk_combo_box_get_active (CTK_COMBO_BOX (data->wp_style_menu));

  pixbuf = cafe_wp_item_get_thumbnail (item, data->thumb_factory,
                                        data->thumb_width,
                                        data->thumb_height);
  ctk_list_store_set (CTK_LIST_STORE (data->wp_model), &iter, 0, pixbuf, -1);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  if (g_settings_is_writable (data->wp_settings, WP_OPTIONS_KEY))
  {
    g_settings_delay (data->wp_settings);
    g_settings_set_enum (data->wp_settings, WP_OPTIONS_KEY, item->options);
    g_settings_apply (data->wp_settings);
  }
}

static void
wp_shade_type_changed (CtkWidget *combobox,
                       AppearanceData *data)
{
  CafeWPItem *item;
  CtkTreeIter iter;
  GdkPixbuf *pixbuf;

  item = get_selected_item (data, &iter);

  if (item == NULL)
    return;

  item->shade_type = ctk_combo_box_get_active (CTK_COMBO_BOX (data->wp_color_menu));

  pixbuf = cafe_wp_item_get_thumbnail (item, data->thumb_factory,
                                        data->thumb_width,
                                        data->thumb_height);
  ctk_list_store_set (CTK_LIST_STORE (data->wp_model), &iter, 0, pixbuf, -1);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  if (g_settings_is_writable (data->wp_settings, WP_SHADING_KEY))
  {
    g_settings_delay (data->wp_settings);
    g_settings_set_enum (data->wp_settings, WP_SHADING_KEY, item->shade_type);
    g_settings_apply (data->wp_settings);
  }
}

static void
wp_color_changed (AppearanceData *data,
                  gboolean update)
{
  CafeWPItem *item;

  item = get_selected_item (data, NULL);

  if (item == NULL)
    return;

  ctk_color_chooser_get_rgba (CTK_COLOR_CHOOSER (data->wp_pcpicker), item->pcolor);
  ctk_color_chooser_get_rgba (CTK_COLOR_CHOOSER (data->wp_scpicker), item->scolor);

  if (update)
  {
    gchar *pcolor, *scolor;

    pcolor = cdk_rgba_to_string (item->pcolor);
    scolor = cdk_rgba_to_string (item->scolor);
    g_settings_delay (data->wp_settings);
    g_settings_set_string (data->wp_settings, WP_PCOLOR_KEY, pcolor);
    g_settings_set_string (data->wp_settings, WP_SCOLOR_KEY, scolor);
    g_settings_apply (data->wp_settings);
    g_free (pcolor);
    g_free (scolor);
  }

  wp_shade_type_changed (NULL, data);
}

static void
wp_scolor_changed (CtkWidget *widget,
                   AppearanceData *data)
{
  wp_color_changed (data, TRUE);
}

static void
wp_remove_wallpaper (CtkWidget *widget,
                     AppearanceData *data)
{
  CafeWPItem *item;
  CtkTreeIter iter;
  CtkTreePath *path;

  item = get_selected_item (data, &iter);

  if (item)
  {
    item->deleted = TRUE;

    if (ctk_list_store_remove (CTK_LIST_STORE (data->wp_model), &iter))
      path = ctk_tree_model_get_path (data->wp_model, &iter);
    else
      path = ctk_tree_path_new_first ();

    ctk_icon_view_select_path (data->wp_view, path);
    ctk_icon_view_set_cursor (data->wp_view, path, NULL, FALSE);
    ctk_tree_path_free (path);
  }
}

static void
wp_uri_changed (const gchar *uri,
                AppearanceData *data)
{
  CafeWPItem *item, *selected;

  item = g_hash_table_lookup (data->wp_hash, uri);
  selected = get_selected_item (data, NULL);

  if (selected != NULL && strcmp (selected->filename, uri) != 0)
  {
    if (item == NULL)
      item = wp_add_image (data, uri);

    if (item)
      select_item (data, item, TRUE);
  }
}

static void
wp_file_changed (GSettings *settings,
                 gchar *key,
                 AppearanceData *data)
{
  gchar *uri;
  gchar *wpfile;

  uri = g_settings_get_string (settings, key);

  if (g_utf8_validate (uri, -1, NULL) && g_file_test (uri, G_FILE_TEST_EXISTS))
    wpfile = g_strdup (uri);
  else
    wpfile = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

  wp_uri_changed (wpfile, data);

  g_free (wpfile);
  g_free (uri);
}

static void
wp_options_changed (GSettings *settings,
                    gchar *key,
                    AppearanceData *data)
{
  CafeWPItem *item;

  item = get_selected_item (data, NULL);

  if (item != NULL)
  {
    item->options = g_settings_get_enum (settings, key);
    wp_option_menu_set (data, item->options, FALSE);
  }
}

static void
wp_shading_changed (GSettings *settings,
                    gchar *key,
                    AppearanceData *data)
{
  CafeWPItem *item;

  wp_set_sensitivities (data);

  item = get_selected_item (data, NULL);

  if (item != NULL)
  {
    item->shade_type = g_settings_get_enum (settings, key);
    wp_option_menu_set (data, item->shade_type, TRUE);
  }
}

static void
wp_color1_changed (GSettings *settings,
                   gchar *key,
                   AppearanceData *data)
{
  CdkRGBA color;
  gchar *colorhex;

  colorhex = g_settings_get_string (settings, key);

  cdk_rgba_parse (&color, colorhex);

  ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (data->wp_pcpicker), &color);

  wp_color_changed (data, FALSE);

  g_free (colorhex);
}

static void
wp_color2_changed (GSettings *settings,
                   gchar *key,
                   AppearanceData *data)
{
  CdkRGBA color;
  gchar *colorhex;

  wp_set_sensitivities (data);

  colorhex = g_settings_get_string (settings, key);

  cdk_rgba_parse (&color, colorhex);

  ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (data->wp_scpicker), &color);

  wp_color_changed (data, FALSE);

  g_free (colorhex);
}

static gboolean
wp_props_wp_set (AppearanceData *data, CafeWPItem *item)
{
  gchar *pcolor, *scolor;

  g_settings_delay (data->wp_settings);

  if (!strcmp (item->filename, "(none)"))
  {
    g_settings_set_enum (data->wp_settings, WP_OPTIONS_KEY, 0);
    g_settings_set_string (data->wp_settings, WP_FILE_KEY, "");
  }
  else
  {
    gchar *uri;

    if (g_utf8_validate (item->filename, -1, NULL))
      uri = g_strdup (item->filename);
    else
      uri = g_filename_to_utf8 (item->filename, -1, NULL, NULL, NULL);

    if (uri == NULL) {
      g_warning ("Failed to convert filename to UTF-8: %s", item->filename);
    } else {
      g_settings_set_string (data->wp_settings, WP_FILE_KEY, uri);
      g_free (uri);
    }

    g_settings_set_enum (data->wp_settings, WP_OPTIONS_KEY, item->options);
  }

  g_settings_set_enum (data->wp_settings, WP_SHADING_KEY, item->shade_type);

  pcolor = cdk_rgba_to_string (item->pcolor);
  scolor = cdk_rgba_to_string (item->scolor);
  g_settings_set_string (data->wp_settings, WP_PCOLOR_KEY, pcolor);
  g_settings_set_string (data->wp_settings, WP_SCOLOR_KEY, scolor);
  g_free (pcolor);
  g_free (scolor);

  g_settings_apply (data->wp_settings);

  return FALSE;
}

static void
wp_props_wp_selected (CtkTreeSelection *selection,
                      AppearanceData *data)
{
  CafeWPItem *item;

  item = get_selected_item (data, NULL);

  if (item != NULL)
  {
    wp_set_sensitivities (data);

    if (strcmp (item->filename, "(none)") != 0)
      wp_option_menu_set (data, item->options, FALSE);

    wp_option_menu_set (data, item->shade_type, TRUE);

    ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (data->wp_pcpicker),
                                item->pcolor);
    ctk_color_chooser_set_rgba (CTK_COLOR_CHOOSER (data->wp_scpicker),
                                item->scolor);

    wp_props_wp_set (data, item);
  }
  else
  {
    ctk_widget_set_sensitive (data->wp_rem_button, FALSE);
  }
}

void
wp_create_filechooser (AppearanceData *data)
{
  const char *start_dir, *pictures = NULL;
  CtkFileFilter *filter;

  data->wp_filesel = CTK_FILE_CHOOSER (
                     ctk_file_chooser_dialog_new (_("Add Wallpaper"),
                     CTK_WINDOW (appearance_capplet_get_widget (data, "appearance_window")),
                     CTK_FILE_CHOOSER_ACTION_OPEN,
                     "ctk-cancel",
                     CTK_RESPONSE_CANCEL,
                     "ctk-open",
                     CTK_RESPONSE_OK,
                     NULL));

  ctk_dialog_set_default_response (CTK_DIALOG (data->wp_filesel), CTK_RESPONSE_OK);
  ctk_file_chooser_set_select_multiple (data->wp_filesel, TRUE);
  ctk_file_chooser_set_use_preview_label (data->wp_filesel, FALSE);

  start_dir = g_get_home_dir ();

  if (g_file_test (BACKGROUND_DATADIR, G_FILE_TEST_IS_DIR)) {
    ctk_file_chooser_add_shortcut_folder (data->wp_filesel,
                                          BACKGROUND_DATADIR, NULL);
    start_dir = BACKGROUND_DATADIR;
  }

  pictures = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures != NULL && g_file_test (pictures, G_FILE_TEST_IS_DIR)) {
    ctk_file_chooser_add_shortcut_folder (data->wp_filesel, pictures, NULL);
    start_dir = pictures;
  }

  ctk_file_chooser_set_current_folder (data->wp_filesel, start_dir);

  filter = ctk_file_filter_new ();
  ctk_file_filter_add_pixbuf_formats (filter);
  ctk_file_filter_set_name (filter, _("Images"));
  ctk_file_chooser_add_filter (data->wp_filesel, filter);

  filter = ctk_file_filter_new ();
  ctk_file_filter_set_name (filter, _("All files"));
  ctk_file_filter_add_pattern (filter, "*");
  ctk_file_chooser_add_filter (data->wp_filesel, filter);

  data->wp_image = ctk_image_new ();
  ctk_file_chooser_set_preview_widget (data->wp_filesel, data->wp_image);
  ctk_widget_set_size_request (data->wp_image, 128, -1);

  ctk_widget_show (data->wp_image);

  g_signal_connect (data->wp_filesel, "update-preview",
                    (GCallback) wp_update_preview, data);
}

static void
wp_file_open_dialog (CtkWidget *widget,
                     AppearanceData *data)
{
  GSList *files;

  if (!data->wp_filesel)
    wp_create_filechooser (data);

  switch (ctk_dialog_run (CTK_DIALOG (data->wp_filesel)))
  {
  case CTK_RESPONSE_OK:
    files = ctk_file_chooser_get_filenames (data->wp_filesel);
    wp_add_images (data, files);
  case CTK_RESPONSE_CANCEL:
  default:
    ctk_widget_hide (CTK_WIDGET (data->wp_filesel));
    break;
  }
}

static void
wp_drag_received (CtkWidget *widget,
                  CdkDragContext *context,
                  gint x, gint y,
                  CtkSelectionData *selection_data,
                  guint info, guint time,
                  AppearanceData *data)
{
  if (info == TARGET_URI_LIST || info == TARGET_BGIMAGE)
  {
    GSList *realuris = NULL;
    gchar **uris;

    uris = g_uri_list_extract_uris ((gchar *) ctk_selection_data_get_data (selection_data));
    if (uris != NULL)
    {
      CtkWidget *w;
      CdkWindow *window;
      CdkCursor *cursor;
      gchar **uri;

      w = appearance_capplet_get_widget (data, "appearance_window");
      window = ctk_widget_get_window (w);

      cursor = cdk_cursor_new_for_display (cdk_display_get_default (),
             CDK_WATCH);
      cdk_window_set_cursor (window, cursor);
      g_object_unref (cursor);

      for (uri = uris; *uri; ++uri)
      {
        GFile *f;

        f = g_file_new_for_uri (*uri);
        realuris = g_slist_append (realuris, g_file_get_path (f));
        g_object_unref (f);
      }

      wp_add_images (data, realuris);
      cdk_window_set_cursor (window, NULL);

      g_strfreev (uris);
    }
  }
}

static void
wp_drag_get_data (CtkWidget *widget,
		  CdkDragContext *context,
		  CtkSelectionData *selection_data,
		  guint type, guint time,
		  AppearanceData *data)
{
  if (type == TARGET_URI_LIST) {
    CafeWPItem *item = get_selected_item (data, NULL);

    if (item != NULL) {
      char *uris[2];

      uris[0] = g_filename_to_uri (item->filename, NULL, NULL);
      uris[1] = NULL;

      ctk_selection_data_set_uris (selection_data, uris);

      g_free (uris[0]);
    }
  }
}

static gboolean
wp_view_tooltip_cb (CtkWidget  *widget,
                    gint x,
                    gint y,
                    gboolean keyboard_mode,
                    CtkTooltip *tooltip,
                    AppearanceData *data)
{
  CtkTreeIter iter;
  CafeWPItem *item;

  if (ctk_icon_view_get_tooltip_context (data->wp_view,
                                         &x, &y,
                                         keyboard_mode,
                                         NULL,
                                         NULL,
                                         &iter))
    {
      ctk_tree_model_get (data->wp_model, &iter, 1, &item, -1);
      ctk_tooltip_set_markup (tooltip, item->description);

      return TRUE;
    }

  return FALSE;
}

static gint
wp_list_sort (CtkTreeModel *model,
              CtkTreeIter *a, CtkTreeIter *b,
              AppearanceData *data)
{
  CafeWPItem *itema, *itemb;
  gint retval;

  ctk_tree_model_get (model, a, 1, &itema, -1);
  ctk_tree_model_get (model, b, 1, &itemb, -1);

  if (!strcmp (itema->filename, "(none)"))
  {
    retval =  -1;
  }
  else if (!strcmp (itemb->filename, "(none)"))
  {
    retval =  1;
  }
  else
  {
    retval = g_utf8_collate (itema->description, itemb->description);
  }

  return retval;
}

static void
wp_update_preview (CtkFileChooser *chooser,
                   AppearanceData *data)
{
  gchar *uri;

  uri = ctk_file_chooser_get_preview_uri (chooser);

  if (uri)
  {
    GdkPixbuf *pixbuf = NULL;
    const gchar *mime_type = NULL;
    GFile *file;
    GFileInfo *file_info;

    file = g_file_new_for_uri (uri);
    file_info = g_file_query_info (file,
                                   G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL, NULL);
    g_object_unref (file);

    if (file_info != NULL)
    {
      mime_type = g_file_info_get_content_type (file_info);
      g_object_unref (file_info);
    }

    if (mime_type)
    {
      pixbuf = cafe_desktop_thumbnail_factory_generate_thumbnail (data->thumb_factory,
								   uri,
								   mime_type);
    }

    if (pixbuf != NULL)
    {
      ctk_image_set_from_pixbuf (CTK_IMAGE (data->wp_image), pixbuf);
      g_object_unref (pixbuf);
    }
    else
    {
      ctk_image_set_from_icon_name (CTK_IMAGE (data->wp_image),
                                    "dialog-question",
                                    CTK_ICON_SIZE_DIALOG);
    }
  }

  ctk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static gboolean
reload_item (CtkTreeModel *model,
             CtkTreePath *path,
             CtkTreeIter *iter,
             AppearanceData *data)
{
  CafeWPItem *item;
  GdkPixbuf *pixbuf;

  ctk_tree_model_get (model, iter, 1, &item, -1);

  pixbuf = cafe_wp_item_get_thumbnail (item,
                                        data->thumb_factory,
                                        data->thumb_width,
                                        data->thumb_height);
  if (pixbuf) {
    ctk_list_store_set (CTK_LIST_STORE (data->wp_model), iter, 0, pixbuf, -1);
    g_object_unref (pixbuf);
  }

  return FALSE;
}

static gdouble
get_monitor_aspect_ratio_for_widget (CtkWidget *widget)
{
  gdouble aspect;
  CdkMonitor *monitor;
  CdkRectangle rect;

  monitor = cdk_display_get_monitor_at_window (ctk_widget_get_display (widget), ctk_widget_get_window (widget));
  cdk_monitor_get_geometry (monitor, &rect);

  aspect = rect.height / (gdouble)rect.width;

  return aspect;
}

#define LIST_IMAGE_SIZE 108

static void
compute_thumbnail_sizes (AppearanceData *data)
{
  gdouble aspect;

  aspect = get_monitor_aspect_ratio_for_widget (CTK_WIDGET (data->wp_view));
  if (aspect > 1) {
    /* portrait */
    data->thumb_width = LIST_IMAGE_SIZE / aspect;
    data->thumb_height = LIST_IMAGE_SIZE;
  } else {
    data->thumb_width = LIST_IMAGE_SIZE;
    data->thumb_height = LIST_IMAGE_SIZE * aspect;
  }
}

static void
reload_wallpapers (AppearanceData *data)
{
  compute_thumbnail_sizes (data);
  ctk_tree_model_foreach (data->wp_model, (CtkTreeModelForeachFunc)reload_item, data);
}

static gboolean
wp_load_stuffs (void *user_data)
{
  AppearanceData *data;
  gchar *imagepath, *uri, *style;
  CafeWPItem *item;

  data = (AppearanceData *) user_data;

  compute_thumbnail_sizes (data);

  cafe_wp_xml_load_list (data);
  g_hash_table_foreach (data->wp_hash, (GHFunc) wp_props_load_wallpaper,
                        data);

  style = g_settings_get_string (data->wp_settings,
                                   WP_OPTIONS_KEY);
  if (style == NULL)
    style = g_strdup ("none");

  uri = g_settings_get_string (data->wp_settings,
                                 WP_FILE_KEY);

  if (uri && *uri == '\0')
  {
    g_free (uri);
    uri = NULL;
  }

  if (uri == NULL)
    uri = g_strdup ("(none)");

  if (g_utf8_validate (uri, -1, NULL) && g_file_test (uri, G_FILE_TEST_EXISTS))
    imagepath = g_strdup (uri);
  else
    imagepath = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

  g_free (uri);

  item = g_hash_table_lookup (data->wp_hash, imagepath);

  if (item != NULL)
  {
    /* update with the current gsettings */
    cafe_wp_item_update (item);

    if (strcmp (style, "none") != 0)
    {
      if (item->deleted == TRUE)
      {
        item->deleted = FALSE;
        wp_props_load_wallpaper (item->filename, item, data);
      }

      select_item (data, item, FALSE);
    }
  }
  else if (strcmp (style, "none") != 0)
  {
    item = wp_add_image (data, imagepath);
    if (item)
      select_item (data, item, FALSE);
  }

  item = g_hash_table_lookup (data->wp_hash, "(none)");
  if (item == NULL)
  {
    item = cafe_wp_item_new ("(none)", data->wp_hash, data->thumb_factory);
    if (item != NULL)
    {
      wp_props_load_wallpaper (item->filename, item, data);
    }
  }
  else
  {
    if (item->deleted == TRUE)
    {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, data);
    }

    if (!strcmp (style, "none"))
    {
      select_item (data, item, FALSE);
      wp_option_menu_set (data, CAFE_BG_PLACEMENT_SCALED, FALSE);
    }
  }
  g_free (imagepath);
  g_free (style);

  if (data->wp_uris) {
    wp_add_images (data, data->wp_uris);
    data->wp_uris = NULL;
  }

  return FALSE;
}

static void
wp_select_after_realize (CtkWidget *widget,
                         AppearanceData *data)
{
  CafeWPItem *item;

  g_idle_add (wp_load_stuffs, data);

  item = get_selected_item (data, NULL);
  if (item == NULL)
    item = g_hash_table_lookup (data->wp_hash, "(none)");

  select_item (data, item, TRUE);
}

static GdkPixbuf *buttons[3];

static void
create_button_images (AppearanceData  *data)
{
  CtkWidget *widget = (CtkWidget*)data->wp_view;
  CtkStyle *style = ctk_widget_get_style (widget);
  CtkIconSet *icon_set;
  GdkPixbuf *pixbuf, *pb, *pb2;
  gint i, w, h;

  icon_set = ctk_style_lookup_icon_set (style, "ctk-media-play");
  pb = ctk_icon_set_render_icon (icon_set,
                                 style,
                                 CTK_TEXT_DIR_RTL,
                                 CTK_STATE_NORMAL,
                                 CTK_ICON_SIZE_MENU,
                                 widget,
                                 NULL);
  pb2 = ctk_icon_set_render_icon (icon_set,
                                  style,
                                  CTK_TEXT_DIR_LTR,
                                  CTK_STATE_NORMAL,
                                  CTK_ICON_SIZE_MENU,
                                  widget,
                                  NULL);
  w = gdk_pixbuf_get_width (pb);
  h = gdk_pixbuf_get_height (pb);

  for (i = 0; i < 3; i++) {
    pixbuf = gdk_pixbuf_new (CDK_COLORSPACE_RGB, TRUE, 8, 2 * w, h);
    gdk_pixbuf_fill (pixbuf, 0);
    if (i > 0)
      gdk_pixbuf_composite (pb, pixbuf, 0, 0, w, h, 0, 0, 1, 1, CDK_INTERP_NEAREST, 255);
    if (i < 2)
      gdk_pixbuf_composite (pb2, pixbuf, w, 0, w, h, w, 0, 1, 1, CDK_INTERP_NEAREST, 255);

    buttons[i] = pixbuf;
  }

  g_object_unref (pb);
  g_object_unref (pb2);
}

static void
next_frame (AppearanceData  *data,
            CtkCellRenderer *cr,
            gint             direction)
{
  CafeWPItem *item;
  CtkTreeIter iter;
  GdkPixbuf *pixbuf, *pb;
  gint frame;

  pixbuf = NULL;

  frame = data->frame + direction;
  item = get_selected_item (data, &iter);

  if (frame >= 0)
    pixbuf = cafe_wp_item_get_frame_thumbnail (item,
                                                data->thumb_factory,
                                                data->thumb_width,
                                                data->thumb_height,
                                                frame);
  if (pixbuf) {
    ctk_list_store_set (CTK_LIST_STORE (data->wp_model), &iter, 0, pixbuf, -1);
    g_object_unref (pixbuf);
    data->frame = frame;
  }

  pb = buttons[1];
  if (direction < 0) {
    if (frame == 0)
      pb = buttons[0];
  }
  else {
    pixbuf = cafe_wp_item_get_frame_thumbnail (item,
                                                data->thumb_factory,
                                                data->thumb_width,
                                                data->thumb_height,
                                                frame + 1);
    if (pixbuf)
      g_object_unref (pixbuf);
    else
      pb = buttons[2];
  }
  g_object_set (cr, "pixbuf", pb, NULL);
}

static gboolean
wp_button_press_cb (CtkWidget      *widget,
                    CdkEventButton *event,
                    AppearanceData *data)
{
  CtkCellRenderer *cell;
  CdkEventButton *button_event = (CdkEventButton *) event;

  if (event->type != CDK_BUTTON_PRESS)
    return FALSE;

  if (ctk_icon_view_get_item_at_pos (CTK_ICON_VIEW (widget),
                                     button_event->x, button_event->y,
                                     NULL, &cell)) {
    if (g_object_get_data (G_OBJECT (cell), "buttons")) {
      gint w, h;
      CtkCellRenderer *cell2 = NULL;
      ctk_icon_size_lookup (CTK_ICON_SIZE_MENU, &w, &h);
      if (ctk_icon_view_get_item_at_pos (CTK_ICON_VIEW (widget),
                                         button_event->x + w, button_event->y,
                                         NULL, &cell2) && cell == cell2)
        next_frame (data, cell, -1);
      else
        next_frame (data, cell, 1);
      return TRUE;
    }
  }

  return FALSE;
}

static void
wp_selected_changed_cb (CtkIconView    *view,
                        AppearanceData *data)
{
  CtkCellRenderer *cr;
  GList *cells, *l;

  data->frame = -1;

  cells = ctk_cell_layout_get_cells (CTK_CELL_LAYOUT (data->wp_view));
  for (l = cells; l; l = l->next) {
    cr = l->data;
    if (g_object_get_data (G_OBJECT (cr), "buttons"))
      g_object_set (cr, "pixbuf", buttons[0], NULL);
  }
  g_list_free (cells);
}

static void
buttons_cell_data_func (CtkCellLayout   *layout,
                        CtkCellRenderer *cell,
                        CtkTreeModel    *model,
                        CtkTreeIter     *iter,
                        gpointer         user_data)
{
  AppearanceData *data = user_data;
  CtkTreePath *path;
  CafeWPItem *item;
  gboolean visible;

  path = ctk_tree_model_get_path (model, iter);

  if (ctk_icon_view_path_is_selected (CTK_ICON_VIEW (layout), path)) {
    item = get_selected_item (data, NULL);
    visible = cafe_bg_changes_with_time (item->bg);
  }
  else
    visible = FALSE;

  g_object_set (G_OBJECT (cell), "visible", visible, NULL);

  ctk_tree_path_free (path);
}

static void
screen_monitors_changed (CdkScreen *screen,
                         AppearanceData *data)
{
  reload_wallpapers (data);
}

void
desktop_init (AppearanceData *data,
	      const gchar **uris)
{
  CtkWidget *add_button, *w;
  CtkCellRenderer *cr;
  char *url;

  data->wp_uris = NULL;
  if (uris != NULL) {
    while (*uris != NULL) {
      data->wp_uris = g_slist_append (data->wp_uris, g_strdup (*uris));
      uris++;
    }
  }

  w = appearance_capplet_get_widget (data, "more_backgrounds_linkbutton");
  url = g_settings_get_string (data->settings, MORE_BACKGROUNDS_URL_KEY);
  if (url != NULL && url[0] != '\0') {
    ctk_link_button_set_uri (CTK_LINK_BUTTON (w), url);
    ctk_widget_show (w);
  } else {
    ctk_widget_hide (w);
  }
  g_free (url);

  data->wp_hash = g_hash_table_new (g_str_hash, g_str_equal);

  g_signal_connect (data->wp_settings,
                           "changed::" WP_FILE_KEY,
                           G_CALLBACK (wp_file_changed),
                           data);
  g_signal_connect (data->wp_settings,
                           "changed::" WP_OPTIONS_KEY,
                           G_CALLBACK (wp_options_changed),
                           data);
  g_signal_connect (data->wp_settings,
                           "changed::" WP_SHADING_KEY,
                           G_CALLBACK (wp_shading_changed),
                           data);
  g_signal_connect (data->wp_settings,
                           "changed::" WP_PCOLOR_KEY,
                           G_CALLBACK (wp_color1_changed),
                           data);
  g_signal_connect (data->wp_settings,
                           "changed::" WP_SCOLOR_KEY,
                           G_CALLBACK (wp_color2_changed),
                           data);

  data->wp_model = CTK_TREE_MODEL (ctk_list_store_new (2, CDK_TYPE_PIXBUF,
                                                       G_TYPE_POINTER));

  data->wp_view = CTK_ICON_VIEW (appearance_capplet_get_widget (data, "wp_view"));
  ctk_icon_view_set_model (data->wp_view, CTK_TREE_MODEL (data->wp_model));

  g_signal_connect_after (data->wp_view, "realize",
                          (GCallback) wp_select_after_realize, data);

  ctk_cell_layout_clear (CTK_CELL_LAYOUT (data->wp_view));

  cr = ctk_cell_renderer_pixbuf_new ();
  g_object_set (cr, "xpad", 5, "ypad", 5, NULL);

  ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (data->wp_view), cr, TRUE);
  ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (data->wp_view), cr,
                                  "pixbuf", 0,
                                  NULL);

  cr = ctk_cell_renderer_pixbuf_new ();
  create_button_images (data);
  g_object_set (cr,
                "mode", CTK_CELL_RENDERER_MODE_ACTIVATABLE,
                "pixbuf", buttons[0],
                NULL);
  g_object_set_data (G_OBJECT (cr), "buttons", GINT_TO_POINTER (TRUE));

  ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (data->wp_view), cr, FALSE);
  ctk_cell_layout_set_cell_data_func (CTK_CELL_LAYOUT (data->wp_view), cr,
                                      buttons_cell_data_func, data, NULL);
  g_signal_connect (data->wp_view, "selection-changed",
                    (GCallback) wp_selected_changed_cb, data);
  g_signal_connect (data->wp_view, "button-press-event",
                    G_CALLBACK (wp_button_press_cb), data);

  data->frame = -1;

  ctk_tree_sortable_set_sort_func (CTK_TREE_SORTABLE (data->wp_model), 1,
                                   (CtkTreeIterCompareFunc) wp_list_sort,
                                   data, NULL);

  ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (data->wp_model),
                                        1, CTK_SORT_ASCENDING);

  ctk_drag_dest_set (CTK_WIDGET (data->wp_view), CTK_DEST_DEFAULT_ALL, drop_types,
                     G_N_ELEMENTS (drop_types), CDK_ACTION_COPY | CDK_ACTION_MOVE);
  g_signal_connect (data->wp_view, "drag_data_received",
                    (GCallback) wp_drag_received, data);

  ctk_drag_source_set (CTK_WIDGET (data->wp_view), CDK_BUTTON1_MASK,
                       drag_types, G_N_ELEMENTS (drag_types), CDK_ACTION_COPY);
  g_signal_connect (data->wp_view, "drag-data-get",
		    (GCallback) wp_drag_get_data, data);

  data->wp_style_menu = appearance_capplet_get_widget (data, "wp_style_menu");

  g_signal_connect (data->wp_style_menu, "changed",
                    (GCallback) wp_scale_type_changed, data);

  data->wp_color_menu = appearance_capplet_get_widget (data, "wp_color_menu");

  g_signal_connect (data->wp_color_menu, "changed",
                    (GCallback) wp_shade_type_changed, data);

  data->wp_scpicker = appearance_capplet_get_widget (data, "wp_scpicker");

  g_signal_connect (data->wp_scpicker, "color-set",
                    (GCallback) wp_scolor_changed, data);

  data->wp_pcpicker = appearance_capplet_get_widget (data, "wp_pcpicker");

  g_signal_connect (data->wp_pcpicker, "color-set",
                    (GCallback) wp_scolor_changed, data);

  add_button = appearance_capplet_get_widget (data, "wp_add_button");
  ctk_button_set_image (CTK_BUTTON (add_button),
                        ctk_image_new_from_icon_name ("list-add", CTK_ICON_SIZE_BUTTON));

  g_signal_connect (add_button, "clicked",
                    (GCallback) wp_file_open_dialog, data);

  data->wp_rem_button = appearance_capplet_get_widget (data, "wp_rem_button");

  g_signal_connect (data->wp_rem_button, "clicked",
                    (GCallback) wp_remove_wallpaper, data);
  data->screen_monitors_handler = g_signal_connect (ctk_widget_get_screen (CTK_WIDGET (data->wp_view)),
                                                    "monitors-changed",
                                                    G_CALLBACK (screen_monitors_changed),
                                                    data);
  data->screen_size_handler = g_signal_connect (ctk_widget_get_screen (CTK_WIDGET (data->wp_view)),
                                                    "size-changed",
                                                    G_CALLBACK (screen_monitors_changed),
                                                    data);

  g_signal_connect (data->wp_view, "selection-changed",
                    (GCallback) wp_props_wp_selected, data);
  g_signal_connect (data->wp_view, "query-tooltip",
                    (GCallback) wp_view_tooltip_cb, data);
  ctk_widget_set_has_tooltip (CTK_WIDGET (data->wp_view), TRUE);

  wp_set_sensitivities (data);

  /* create the file selector later to save time on startup */
  data->wp_filesel = NULL;

}

void
desktop_shutdown (AppearanceData *data)
{
  cafe_wp_xml_save_list (data);

  if (data->screen_monitors_handler > 0) {
    g_signal_handler_disconnect (ctk_widget_get_screen (CTK_WIDGET (data->wp_view)),
                                 data->screen_monitors_handler);
    data->screen_monitors_handler = 0;
  }
  if (data->screen_size_handler > 0) {
    g_signal_handler_disconnect (ctk_widget_get_screen (CTK_WIDGET (data->wp_view)),
                                 data->screen_size_handler);
    data->screen_size_handler = 0;
  }

  g_slist_foreach (data->wp_uris, (GFunc) g_free, NULL);
  g_slist_free (data->wp_uris);
  if (data->wp_filesel)
  {
    g_object_ref_sink (data->wp_filesel);
    g_object_unref (data->wp_filesel);
  }
}
