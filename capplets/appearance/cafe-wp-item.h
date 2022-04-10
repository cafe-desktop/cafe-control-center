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

#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libcafe-desktop/cafe-desktop-thumbnail.h>
#include <libcafe-desktop/cafe-bg.h>

#include "cafe-wp-info.h"

#ifndef _MATE_WP_ITEM_H_
#define _MATE_WP_ITEM_H_

typedef struct _CafeWPItem CafeWPItem;

struct _CafeWPItem {
  CafeBG *bg;

  gchar * name;
  gchar * filename;
  gchar * description;
  CafeBGPlacement options;
  CafeBGColorType shade_type;

  /* Where the Item is in the List */
  GtkTreeRowReference * rowref;

  /* Real colors */
  GdkRGBA * pcolor;
  GdkRGBA * scolor;

  CafeWPInfo * fileinfo;

  /* Did the user remove us? */
  gboolean deleted;

  /* Wallpaper author, if present */
  gchar *artist;

  /* Width and Height of the original image */
  gint width;
  gint height;
};

CafeWPItem * cafe_wp_item_new (const gchar *filename,
				 GHashTable *wallpapers,
				 CafeDesktopThumbnailFactory *thumbnails);

void cafe_wp_item_free (CafeWPItem *item);
GdkPixbuf * cafe_wp_item_get_thumbnail (CafeWPItem *item,
					 CafeDesktopThumbnailFactory *thumbs,
                                         gint width,
                                         gint height);
GdkPixbuf * cafe_wp_item_get_frame_thumbnail (CafeWPItem *item,
                                               CafeDesktopThumbnailFactory *thumbs,
                                               gint width,
                                               gint height,
                                               gint frame);
void cafe_wp_item_update (CafeWPItem *item);
void cafe_wp_item_update_description (CafeWPItem *item);
void cafe_wp_item_ensure_cafe_bg (CafeWPItem *item);

const gchar *wp_item_option_to_string (CafeBGPlacement type);
const gchar *wp_item_shading_to_string (CafeBGColorType type);
CafeBGPlacement wp_item_string_to_option (const gchar *option);
CafeBGColorType wp_item_string_to_shading (const gchar *shade_type);

#endif
