/*
 * This file is part of libtile.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libtile is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libtile is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "nameplate-tile.h"

static void nameplate_tile_get_property (GObject *, guint, GValue *, GParamSpec *);
static void nameplate_tile_set_property (GObject *, guint, const GValue *, GParamSpec *);
static GObject *nameplate_tile_constructor (GType, guint, GObjectConstructParam *);

static void nameplate_tile_drag_begin (CtkWidget *, CdkDragContext *);

static void nameplate_tile_setup (NameplateTile *);

typedef struct
{
	CtkContainer *image_ctnr;
	CtkContainer *header_ctnr;
	CtkContainer *subheader_ctnr;
} NameplateTilePrivate;

enum
{
	PROP_0,
	PROP_NAMEPLATE_IMAGE,
	PROP_NAMEPLATE_HEADER,
	PROP_NAMEPLATE_SUBHEADER,
};

G_DEFINE_TYPE_WITH_PRIVATE (NameplateTile, nameplate_tile, TILE_TYPE)

CtkWidget *nameplate_tile_new (const gchar * uri, CtkWidget * image, CtkWidget * header,
	CtkWidget * subheader)
{
	return CTK_WIDGET (
		g_object_new (NAMEPLATE_TILE_TYPE,
		"tile-uri",            uri,
		"nameplate-image",     image,
		"nameplate-header",    header,
		"nameplate-subheader", subheader,
		NULL));
}

static void
nameplate_tile_class_init (NameplateTileClass * this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);
	CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (this_class);

	g_obj_class->constructor = nameplate_tile_constructor;
	g_obj_class->get_property = nameplate_tile_get_property;
	g_obj_class->set_property = nameplate_tile_set_property;

	widget_class->drag_begin = nameplate_tile_drag_begin;

	g_object_class_install_property (g_obj_class, PROP_NAMEPLATE_IMAGE,
		g_param_spec_object ("nameplate-image", "nameplate-image", "nameplate image",
			CTK_TYPE_WIDGET, G_PARAM_READWRITE));

	g_object_class_install_property (g_obj_class, PROP_NAMEPLATE_HEADER,
		g_param_spec_object ("nameplate-header", "nameplate-header", "nameplate header",
			CTK_TYPE_WIDGET, G_PARAM_READWRITE));

	g_object_class_install_property (g_obj_class, PROP_NAMEPLATE_SUBHEADER,
		g_param_spec_object ("nameplate-subheader", "nameplate-subheader",
			"nameplate subheader", CTK_TYPE_WIDGET, G_PARAM_READWRITE));
}

static void
nameplate_tile_init (NameplateTile * this)
{
}

static GObject *
nameplate_tile_constructor (GType type, guint n_param, GObjectConstructParam * param)
{
	GObject *g_obj =
		(*G_OBJECT_CLASS (nameplate_tile_parent_class)->constructor) (type, n_param, param);

	nameplate_tile_setup (NAMEPLATE_TILE (g_obj));

	return g_obj;
}

static void
nameplate_tile_get_property (GObject * g_object, guint prop_id, GValue * value,
	GParamSpec * param_spec)
{
	NameplateTile *np_tile = NAMEPLATE_TILE (g_object);

	switch (prop_id)
	{
	case PROP_NAMEPLATE_IMAGE:
		g_value_set_object (value, np_tile->image);
		break;

	case PROP_NAMEPLATE_HEADER:
		g_value_set_object (value, np_tile->header);
		break;

	case PROP_NAMEPLATE_SUBHEADER:
		g_value_set_object (value, np_tile->subheader);
		break;
	default:
		break;
	}
}

static void
nameplate_tile_set_property (GObject * g_object, guint prop_id, const GValue * value,
	GParamSpec * param_spec)
{
	NameplateTile *this = NAMEPLATE_TILE (g_object);
	NameplateTilePrivate *priv = nameplate_tile_get_instance_private (this);

	GObject *widget_obj = NULL;

	switch (prop_id) {
		case PROP_NAMEPLATE_IMAGE:
		case PROP_NAMEPLATE_HEADER:
		case PROP_NAMEPLATE_SUBHEADER:
			widget_obj = g_value_get_object (value);
			break;
		default:
			break;
	}

	switch (prop_id)
	{
	case PROP_NAMEPLATE_IMAGE:
		if (CTK_IS_WIDGET (widget_obj))
		{
			if (CTK_IS_WIDGET (this->image))
				ctk_widget_destroy (this->image);

			this->image = CTK_WIDGET (widget_obj);

			ctk_container_add (priv->image_ctnr, this->image);

			ctk_widget_show_all (this->image);
		}
		else if (CTK_IS_WIDGET (this->image))
			ctk_widget_destroy (this->image);

		break;

	case PROP_NAMEPLATE_HEADER:
		if (CTK_IS_WIDGET (widget_obj))
		{
			if (CTK_IS_WIDGET (this->header))
				ctk_widget_destroy (this->header);

			this->header = CTK_WIDGET (widget_obj);

			ctk_container_add (priv->header_ctnr, this->header);

			ctk_widget_show_all (this->header);
		}
		else if (CTK_IS_WIDGET (this->header))
			ctk_widget_destroy (this->header);

		break;

	case PROP_NAMEPLATE_SUBHEADER:
		if (CTK_IS_WIDGET (widget_obj))
		{
			if (CTK_IS_WIDGET (this->subheader))
				ctk_widget_destroy (this->subheader);

			this->subheader = CTK_WIDGET (widget_obj);

			ctk_container_add (priv->subheader_ctnr, this->subheader);

			ctk_widget_show_all (this->subheader);
		}
		else if (CTK_IS_WIDGET (this->subheader))
			ctk_widget_destroy (this->subheader);

		break;

	default:
		break;
	}
}

static void
nameplate_tile_setup (NameplateTile *this)
{
	NameplateTilePrivate *priv = nameplate_tile_get_instance_private (this);

	CtkWidget *hbox;
	CtkWidget *vbox;

	priv->image_ctnr = CTK_CONTAINER (ctk_box_new (CTK_ORIENTATION_VERTICAL, 0));
	ctk_widget_set_valign (CTK_WIDGET (priv->image_ctnr), CTK_ALIGN_CENTER);

	priv->header_ctnr = CTK_CONTAINER (ctk_box_new (CTK_ORIENTATION_VERTICAL, 0));

	priv->subheader_ctnr = CTK_CONTAINER (ctk_box_new (CTK_ORIENTATION_VERTICAL, 0));
	ctk_widget_set_halign (CTK_WIDGET (priv->subheader_ctnr), CTK_ALIGN_START);

	hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
	vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_set_halign (vbox, CTK_ALIGN_FILL);
	ctk_widget_set_valign (vbox, CTK_ALIGN_CENTER);

	ctk_container_add (CTK_CONTAINER (this), hbox);
	ctk_box_pack_start (CTK_BOX (hbox), CTK_WIDGET (priv->image_ctnr), FALSE, FALSE, 0);
	ctk_box_pack_start (CTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	ctk_box_pack_start (CTK_BOX (vbox), CTK_WIDGET (priv->header_ctnr), FALSE, FALSE, 0);
	ctk_box_pack_start (CTK_BOX (vbox), CTK_WIDGET (priv->subheader_ctnr), FALSE, FALSE, 0);

	if (CTK_IS_WIDGET (this->image))
		ctk_container_add (priv->image_ctnr, this->image);

	if (CTK_IS_WIDGET (this->header))
		ctk_container_add (priv->header_ctnr, this->header);

	if (CTK_IS_WIDGET (this->subheader))
		ctk_container_add (priv->subheader_ctnr, this->subheader);

	ctk_widget_set_focus_on_click (CTK_WIDGET (this), FALSE);
}

static void
nameplate_tile_drag_begin (CtkWidget * widget, CdkDragContext * context)
{
	NameplateTile *this = NAMEPLATE_TILE (widget);
	CtkImage *image;
	const gchar *name;

	(*CTK_WIDGET_CLASS (nameplate_tile_parent_class)->drag_begin) (widget, context);

	if (!this->image || !CTK_IS_IMAGE (this->image))
		return;

	image = CTK_IMAGE (this->image);

	switch (ctk_image_get_storage_type (image))
	{
	case CTK_IMAGE_PIXBUF:
		if (ctk_image_get_pixbuf (image))
			ctk_drag_set_icon_pixbuf (context, ctk_image_get_pixbuf (image), 0, 0);

		break;

	case CTK_IMAGE_ICON_NAME:
		ctk_image_get_icon_name (image, &name, NULL);
		if (name)
			ctk_drag_set_icon_name (context, name, 0, 0);

		break;

	default:
		break;
	}
}
