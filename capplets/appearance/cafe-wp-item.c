/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02110-1301, USA.
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>
#include "appearance.h"
#include "cafe-wp-item.h"

const gchar *wp_item_option_to_string (CafeBGPlacement type)
{
  switch (type)
  {
    case CAFE_BG_PLACEMENT_CENTERED:
      return "centered";
      break;
    case CAFE_BG_PLACEMENT_FILL_SCREEN:
      return "stretched";
      break;
    case CAFE_BG_PLACEMENT_SCALED:
      return "scaled";
      break;
    case CAFE_BG_PLACEMENT_ZOOMED:
      return "zoom";
      break;
    case CAFE_BG_PLACEMENT_TILED:
      return "wallpaper";
      break;
    case CAFE_BG_PLACEMENT_SPANNED:
      return "spanned";
      break;
  }
  return "";
}

const gchar *wp_item_shading_to_string (CafeBGColorType type)
{
  switch (type) {
    case CAFE_BG_COLOR_SOLID:
      return "solid";
      break;
    case CAFE_BG_COLOR_H_GRADIENT:
      return "horizontal-gradient";
      break;
    case CAFE_BG_COLOR_V_GRADIENT:
      return "vertical-gradient";
      break;
  }
  return "";
}

CafeBGPlacement wp_item_string_to_option (const gchar *option)
{
  if (!g_strcmp0(option, "centered"))
    return CAFE_BG_PLACEMENT_CENTERED;
  else if (!g_strcmp0(option, "stretched"))
    return CAFE_BG_PLACEMENT_FILL_SCREEN;
  else if (!g_strcmp0(option, "scaled"))
    return CAFE_BG_PLACEMENT_SCALED;
  else if (!g_strcmp0(option, "zoom"))
    return CAFE_BG_PLACEMENT_ZOOMED;
  else if (!g_strcmp0(option, "wallpaper"))
    return CAFE_BG_PLACEMENT_TILED;
  else if (!g_strcmp0(option, "spanned"))
    return CAFE_BG_PLACEMENT_SPANNED;
  else
    return CAFE_BG_PLACEMENT_SCALED;
}

CafeBGColorType wp_item_string_to_shading (const gchar *shade_type)
{
  if (!g_strcmp0(shade_type, "solid"))
    return CAFE_BG_COLOR_SOLID;
  else if (!g_strcmp0(shade_type, "horizontal-gradient"))
    return CAFE_BG_COLOR_H_GRADIENT;
  else if (!g_strcmp0(shade_type, "vertical-gradient"))
    return CAFE_BG_COLOR_V_GRADIENT;
  else
    return CAFE_BG_COLOR_SOLID;
}

static void set_bg_properties (CafeWPItem *item)
{
  if (item->filename)
    cafe_bg_set_filename (item->bg, item->filename);

  cafe_bg_set_color (item->bg, item->shade_type, item->pcolor, item->scolor);
  cafe_bg_set_placement (item->bg, item->options);
}

void cafe_wp_item_ensure_cafe_bg (CafeWPItem *item)
{
  if (!item->bg) {
    item->bg = cafe_bg_new ();

    set_bg_properties (item);
  }
}

void cafe_wp_item_update (CafeWPItem *item) {
  GSettings *settings;
  CdkRGBA color1 = { 0, 0, 0, 1.0 }, color2 = { 0, 0, 0, 1.0 };
  gchar *s;

  settings = g_settings_new (WP_SCHEMA);

  item->options = g_settings_get_enum (settings, WP_OPTIONS_KEY);

  item->shade_type = g_settings_get_enum (settings, WP_SHADING_KEY);

  s = g_settings_get_string (settings, WP_PCOLOR_KEY);
  if (s != NULL) {
    cdk_rgba_parse (&color1, s);
    g_free (s);
  }

  s = g_settings_get_string (settings, WP_SCOLOR_KEY);
  if (s != NULL) {
    cdk_rgba_parse (&color2, s);
    g_free (s);
  }

  g_object_unref (settings);

  if (item->pcolor != NULL)
    cdk_rgba_free (item->pcolor);

  if (item->scolor != NULL)
    cdk_rgba_free (item->scolor);

  item->pcolor = cdk_rgba_copy (&color1);
  item->scolor = cdk_rgba_copy (&color2);
}

CafeWPItem * cafe_wp_item_new (const gchar * filename,
				 GHashTable * wallpapers,
				 CafeDesktopThumbnailFactory * thumbnails) {
  CafeWPItem *item = g_new0 (CafeWPItem, 1);

  item->filename = g_strdup (filename);
  item->fileinfo = cafe_wp_info_new (filename, thumbnails);

  if (item->fileinfo != NULL && item->fileinfo->mime_type != NULL &&
      (g_str_has_prefix (item->fileinfo->mime_type, "image/") ||
       strcmp (item->fileinfo->mime_type, "application/xml") == 0)) {

    if (g_utf8_validate (item->fileinfo->name, -1, NULL))
      item->name = g_strdup (item->fileinfo->name);
    else
      item->name = g_filename_to_utf8 (item->fileinfo->name, -1, NULL,
				       NULL, NULL);

    cafe_wp_item_update (item);
    cafe_wp_item_ensure_cafe_bg (item);
    cafe_wp_item_update_description (item);

    g_hash_table_insert (wallpapers, item->filename, item);
  } else {
    cafe_wp_item_free (item);
    item = NULL;
  }

  return item;
}

void cafe_wp_item_free (CafeWPItem * item) {
  if (item == NULL) {
    return;
  }

  g_free (item->name);
  g_free (item->filename);
  g_free (item->description);

  if (item->pcolor != NULL)
    cdk_rgba_free (item->pcolor);

  if (item->scolor != NULL)
    cdk_rgba_free (item->scolor);

  cafe_wp_info_free (item->fileinfo);
  if (item->bg)
    g_object_unref (item->bg);

  ctk_tree_row_reference_free (item->rowref);

  g_free (item);
}

static GdkPixbuf *
add_slideshow_frame (GdkPixbuf *pixbuf)
{
  GdkPixbuf *sheet, *sheet2;
  GdkPixbuf *tmp;
  gint w, h;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  sheet = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, w, h);
  gdk_pixbuf_fill (sheet, 0x00000000);
  sheet2 = gdk_pixbuf_new_subpixbuf (sheet, 1, 1, w - 2, h - 2);
  gdk_pixbuf_fill (sheet2, 0xffffffff);
  g_object_unref (sheet2);

  tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w + 6, h + 6);

  gdk_pixbuf_fill (tmp, 0x00000000);
  gdk_pixbuf_composite (sheet, tmp, 6, 6, w, h, 6.0, 6.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
  gdk_pixbuf_composite (sheet, tmp, 3, 3, w, h, 3.0, 3.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
  gdk_pixbuf_composite (pixbuf, tmp, 0, 0, w, h, 0.0, 0.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);

  g_object_unref (sheet);

  return tmp;
}

GdkPixbuf * cafe_wp_item_get_frame_thumbnail (CafeWPItem * item,
					       CafeDesktopThumbnailFactory * thumbs,
                                               int width,
                                               int height,
                                               gint frame) {
  GdkPixbuf *pixbuf = NULL;

  set_bg_properties (item);

  if (frame != -1)
    pixbuf = cafe_bg_create_frame_thumbnail (item->bg, thumbs, cdk_screen_get_default (), width, height, frame);
  else
    pixbuf = cafe_bg_create_thumbnail (item->bg, thumbs, cdk_screen_get_default(), width, height);

  if (pixbuf && cafe_bg_changes_with_time (item->bg))
    {
      GdkPixbuf *tmp;

      tmp = add_slideshow_frame (pixbuf);
      g_object_unref (pixbuf);
      pixbuf = tmp;
    }

  cafe_bg_get_image_size (item->bg, thumbs, width, height, &item->width, &item->height);

  return pixbuf;
}


GdkPixbuf * cafe_wp_item_get_thumbnail (CafeWPItem * item,
					 CafeDesktopThumbnailFactory * thumbs,
                                         gint width,
                                         gint height) {
  return cafe_wp_item_get_frame_thumbnail (item, thumbs, width, height, -1);
}

void cafe_wp_item_update_description (CafeWPItem * item) {
  g_free (item->description);

  if (!strcmp (item->filename, "(none)")) {
    item->description = g_strdup (item->name);
  } else {
    const gchar *description;
    gchar *size;
    gchar *dirname = g_path_get_dirname (item->filename);
    gchar *artist;

    description = NULL;
    size = NULL;

    if (!item->artist || item->artist[0] == 0 || !g_strcmp0(item->artist, "(none)"))
      artist = g_strdup (_("unknown"));
    else
      artist = g_strdup (item->artist);

    if (strcmp (item->fileinfo->mime_type, "application/xml") == 0)
      {
        if (cafe_bg_changes_with_time (item->bg))
          description = _("Slide Show");
        else if (item->width > 0 && item->height > 0)
          description = _("Image");
      }
    else
      description = g_content_type_get_description (item->fileinfo->mime_type);

    if (cafe_bg_has_multiple_sizes (item->bg))
      size = g_strdup (_("multiple sizes"));
    else if (item->width > 0 && item->height > 0) {
      /* translators: x pixel(s) by y pixel(s) */
      size = g_strdup_printf (_("%d %s by %d %s"),
                              item->width,
                              ngettext ("pixel", "pixels", item->width),
                              item->height,
                              ngettext ("pixel", "pixels", item->height));
    }

    if (description && size) {
      /* translators: <b>wallpaper name</b>
       * mime type, size
       * Folder: /path/to/file
       * Artist: wallpaper author
       */
      item->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                                     "%s, %s\n"
                                                     "Folder: %s\n"
                                                     "Artist: %s"),
                                                   item->name,
                                                   description,
                                                   size,
                                                   dirname,
                                                   artist);
    } else {
      /* translators: <b>wallpaper name</b>
       * Image missing
       * Folder: /path/to/file
       * Artist: wallpaper author
       */
      item->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                                     "%s\n"
                                                     "Folder: %s\n"
                                                     "Artist: %s"),
                                                   item->name,
                                                   _("Image missing"),
                                                   dirname,
                                                   artist);
    }

    g_free (size);
    g_free (dirname);
    g_free (artist);
  }
}
