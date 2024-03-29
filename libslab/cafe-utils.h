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

#ifndef __CAFE_UTILS_H__
#define __CAFE_UTILS_H__

#include <ctk/ctk.h>

#ifdef __cplusplus
extern "C" {
#endif

gboolean load_image_by_id (CtkImage * image, CtkIconSize size,
	const gchar * image_id);

#ifdef __cplusplus
}
#endif
#endif /* __CAFE_UTILS_H__ */
