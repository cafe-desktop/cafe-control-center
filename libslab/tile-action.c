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

#include "tile.h"

G_DEFINE_TYPE (TileAction, tile_action, G_TYPE_OBJECT)

static void tile_action_finalize (GObject *);
static void tile_action_menu_item_activate_cb (CtkMenuItem *, gpointer);

static void tile_action_class_init (TileActionClass * this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	g_obj_class->finalize = tile_action_finalize;
}

static void
tile_action_init (TileAction * this)
{
	this->tile = NULL;
	this->func = 0;
	this->menu_item = NULL;
	this->flags = 0;
}

static void
tile_action_finalize (GObject * g_object)
{
	TileAction *action = TILE_ACTION (g_object);
	if (action->menu_item)
		ctk_widget_destroy (CTK_WIDGET (action->menu_item));

	(*G_OBJECT_CLASS (tile_action_parent_class)->finalize) (g_object);
}

TileAction *
tile_action_new (Tile * tile, TileActionFunc func, const gchar * menu_item_markup, guint32 flags)
{
	TileAction *this = g_object_new (TILE_ACTION_TYPE, NULL);

	this->tile = tile;
	this->func = func;

	if (menu_item_markup)
		tile_action_set_menu_item_label (this, menu_item_markup);
	else
		this->menu_item = NULL;

	this->flags = flags;

	return this;
}

void
tile_action_set_menu_item_label (TileAction * this, const gchar * markup)
{
	CtkWidget *label;

	if (this->menu_item)
	{
		label = ctk_bin_get_child (CTK_BIN (this->menu_item));
		ctk_label_set_markup (CTK_LABEL (label), markup);
	}
	else
	{
		label = ctk_label_new (markup);
		ctk_label_set_use_markup (CTK_LABEL (label), TRUE);
		ctk_label_set_xalign (CTK_LABEL (label), 0.0);

		this->menu_item = CTK_MENU_ITEM (ctk_menu_item_new ());
		ctk_container_add (CTK_CONTAINER (this->menu_item), label);

		g_signal_connect (G_OBJECT (this->menu_item), "activate",
			G_CALLBACK (tile_action_menu_item_activate_cb), this);
	}
}

CtkMenuItem *
tile_action_get_menu_item (TileAction * this)
{
	return this->menu_item;
}

static void
tile_action_menu_item_activate_cb (CtkMenuItem * menu_item, gpointer user_data)
{
	TileAction *this = TILE_ACTION (user_data);

	tile_trigger_action (this->tile, this);
}
