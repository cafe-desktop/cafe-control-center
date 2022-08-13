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

#ifndef __SLAB_CAFE_UTIL_H__
#define __SLAB_CAFE_UTIL_H__

#include <glib.h>
#include <ctk/ctk.h>
#include <libcafe-desktop/cafe-desktop-item.h>

#ifdef __cplusplus
extern "C" {
#endif

CafeDesktopItem *load_desktop_item_from_unknown (const gchar * id);

gboolean open_desktop_item_exec (CafeDesktopItem * desktop_item);
gboolean open_desktop_item_help (CafeDesktopItem * desktop_item);

void copy_file (const gchar * src_uri, const gchar * dst_uri);

#ifdef __cplusplus
}
#endif
#endif /* __SLAB_CAFE_UTIL_H__ */
