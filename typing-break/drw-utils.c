/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Richard Hult <richard@imendio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <cdk/cdk.h>
#include <cdk/cdkx.h>
#include <ctk/ctk.h>
#include "drw-utils.h"

static CdkPixbuf *
create_tile_pixbuf (CdkPixbuf    *dest_pixbuf,
		    CdkPixbuf    *src_pixbuf,
		    CdkRectangle *field_geom,
		    guint         alpha,
		    CdkColor     *bg_color)
{
	gboolean need_composite;
	gboolean use_simple;
	gdouble  cx, cy;
	gdouble  colorv;
	gint     pwidth, pheight;

	need_composite = (alpha < 255 || cdk_pixbuf_get_has_alpha (src_pixbuf));
	use_simple = (dest_pixbuf == NULL);

	if (dest_pixbuf == NULL)
		dest_pixbuf = cdk_pixbuf_new (CDK_COLORSPACE_RGB,
					      FALSE, 8,
					      field_geom->width, field_geom->height);

	if (need_composite && use_simple)
		colorv = ((bg_color->red & 0xff00) << 8) |
			(bg_color->green & 0xff00) |
			((bg_color->blue & 0xff00) >> 8);
	else
		colorv = 0;

	pwidth = cdk_pixbuf_get_width (src_pixbuf);
	pheight = cdk_pixbuf_get_height (src_pixbuf);

	for (cy = 0; cy < field_geom->height; cy += pheight) {
		for (cx = 0; cx < field_geom->width; cx += pwidth) {
			if (need_composite && !use_simple)
				cdk_pixbuf_composite (src_pixbuf, dest_pixbuf,
						      cx, cy,
						      MIN (pwidth, field_geom->width - cx),
						      MIN (pheight, field_geom->height - cy),
						      cx, cy,
						      1.0, 1.0,
						      CDK_INTERP_BILINEAR,
						      alpha);
			else if (need_composite && use_simple)
				cdk_pixbuf_composite_color (src_pixbuf, dest_pixbuf,
							    cx, cy,
							    MIN (pwidth, field_geom->width - cx),
							    MIN (pheight, field_geom->height - cy),
							    cx, cy,
							    1.0, 1.0,
							    CDK_INTERP_BILINEAR,
							    alpha,
							    65536, 65536, 65536,
							    colorv, colorv);
			else
				cdk_pixbuf_copy_area (src_pixbuf,
						      0, 0,
						      MIN (pwidth, field_geom->width - cx),
						      MIN (pheight, field_geom->height - cy),
						      dest_pixbuf,
						      cx, cy);
		}
	}

	return dest_pixbuf;
}

static gboolean
window_draw_event   (CtkWidget      *widget,
		     cairo_t        *cr,
		     gpointer        data)
{
	int              width;
	int              height;

	ctk_window_get_size (CTK_WINDOW (widget), &width, &height);

	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_paint (cr);

	/* draw a box */
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_set_source_rgba (cr, 0.2, 0.2, 0.2, 0.5);
	cairo_fill (cr);

	return FALSE;
}

static void
set_pixmap_background (CtkWidget *window)
{
	CdkScreen    *screen;
	CdkPixbuf    *tmp_pixbuf, *pixbuf, *tile_pixbuf;
	CdkRectangle  rect;
	CdkColor      color;
	gint          width, height, scale;
	cairo_t      *cr;

	ctk_widget_realize (window);

	screen = ctk_widget_get_screen (window);
	scale = ctk_widget_get_scale_factor (window);
	width = WidthOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale;
	height = HeightOfScreen (cdk_x11_screen_get_xscreen (screen)) / scale;

	tmp_pixbuf = cdk_pixbuf_get_from_window (cdk_screen_get_root_window (screen),
						 0,
						 0,
						 width, height);

	pixbuf = cdk_pixbuf_new_from_file (IMAGEDIR "/ocean-stripes.png", NULL);

	rect.x = 0;
	rect.y = 0;
	rect.width = width;
	rect.height = height;

	color.red = 0;
	color.blue = 0;
	color.green = 0;

	tile_pixbuf = create_tile_pixbuf (NULL,
					  pixbuf,
					  &rect,
					  155,
					  &color);

	g_object_unref (pixbuf);

	cdk_pixbuf_composite (tile_pixbuf,
			      tmp_pixbuf,
			      0,
			      0,
			      width,
			      height,
			      0,
			      0,
			      scale,
			      scale,
			      CDK_INTERP_NEAREST,
			      225);

	g_object_unref (tile_pixbuf);

	cr = cdk_cairo_create (ctk_widget_get_window (window));
	cdk_cairo_set_source_pixbuf (cr, tmp_pixbuf, 0, 0);
	cairo_paint (cr);

	g_object_unref (tmp_pixbuf);

	cairo_destroy (cr);
}

void
drw_setup_background (CtkWidget *window)
{
	CdkScreen    *screen;
	gboolean      is_composited;

	screen = ctk_widget_get_screen (window);
	is_composited = cdk_screen_is_composited (screen);

	if (is_composited) {
		g_signal_connect (window, "draw", G_CALLBACK (window_draw_event), window);
	} else {
		set_pixmap_background (window);
	}
}

