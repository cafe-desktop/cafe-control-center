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

#include "search-bar.h"
#include "config.h"

#include "nld-marshal.h"

#include <glib/gi18n-lib.h>

typedef struct
{
	CtkWidget *hbox;
	CtkEntry *entry;
	CtkWidget *button;

	int search_timeout;
	guint timeout_id;

	gboolean block_signal;
} NldSearchBarPrivate;

static void nld_search_bar_finalize (GObject *);

static gboolean nld_search_bar_focus (CtkWidget *, CtkDirectionType);
static void nld_search_bar_grab_focus (CtkWidget *);

enum
{
	SEARCH,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (NldSearchBar, nld_search_bar, CTK_TYPE_BOX)

static void emit_search (NldSearchBar * search_bar);
static void emit_search_callback (CtkWidget * widget, gpointer search_bar);

static void nld_search_bar_class_init (NldSearchBarClass * nld_search_bar_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (nld_search_bar_class);
	CtkWidgetClass *widget_class = CTK_WIDGET_CLASS (nld_search_bar_class);

	object_class->finalize = nld_search_bar_finalize;
	widget_class->focus = nld_search_bar_focus;
	widget_class->grab_focus = nld_search_bar_grab_focus;

	signals[SEARCH] =
		g_signal_new ("search", G_TYPE_FROM_CLASS (nld_search_bar_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (NldSearchBarClass, search),
		NULL, NULL, nld_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
nld_search_bar_init (NldSearchBar * search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);
	CtkWidget *entry;

	ctk_widget_set_can_focus (CTK_WIDGET (search_bar), TRUE);
	ctk_orientable_set_orientation (CTK_ORIENTABLE (search_bar), CTK_ORIENTATION_VERTICAL);

	priv->hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 3);
	ctk_box_pack_start (CTK_BOX (search_bar), priv->hbox, TRUE, FALSE, 0);

	entry = ctk_search_entry_new ();
	ctk_widget_set_halign (entry, CTK_ALIGN_START);
	ctk_widget_set_valign (entry, CTK_ALIGN_CENTER);
	priv->entry = CTK_ENTRY (entry);
	ctk_widget_show (entry);
	ctk_box_pack_start (CTK_BOX (priv->hbox), entry, TRUE, TRUE, 0);

	g_signal_connect (entry, "activate", G_CALLBACK (emit_search_callback), search_bar);

	priv->search_timeout = -1;
}

static void
nld_search_bar_finalize (GObject * object)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (NLD_SEARCH_BAR(object));

	if (priv->timeout_id)
		g_source_remove (priv->timeout_id);

	G_OBJECT_CLASS (nld_search_bar_parent_class)->finalize (object);
}

static gboolean
nld_search_bar_focus (CtkWidget * widget, CtkDirectionType dir)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (NLD_SEARCH_BAR(widget));

	return ctk_widget_child_focus (priv->hbox, dir);
}

gboolean
nld_search_bar_has_focus (NldSearchBar * search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	return ctk_widget_has_focus (CTK_WIDGET (priv->entry));
}

static void
nld_search_bar_grab_focus (CtkWidget * widget)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (NLD_SEARCH_BAR(widget));

	ctk_widget_grab_focus (CTK_WIDGET (priv->entry));
}

CtkWidget *
nld_search_bar_new (void)
{
	return g_object_new (NLD_TYPE_SEARCH_BAR, NULL);
}

void
nld_search_bar_clear (NldSearchBar * search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	priv->block_signal = TRUE;
	ctk_entry_set_text (priv->entry, "");
	priv->block_signal = FALSE;
}

static void
emit_search (NldSearchBar * search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	if (priv->block_signal)
		return;

	if (priv->timeout_id)
	{
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	g_signal_emit (search_bar, signals[SEARCH], 0,
		nld_search_bar_get_text (search_bar));
}

static void
emit_search_callback (CtkWidget * widget, gpointer search_bar)
{
	emit_search (search_bar);
}

static gboolean
search_timeout (gpointer search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	priv->timeout_id = 0;
	emit_search (search_bar);
	return FALSE;
}

static void
entry_changed (CtkWidget * entry, gpointer search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	if (priv->search_timeout == 0)
		emit_search (search_bar);
	else if (priv->search_timeout > 0)
	{
		if (priv->timeout_id != 0)
			g_source_remove (priv->timeout_id);
		priv->timeout_id =
			g_timeout_add (priv->search_timeout * 1000, search_timeout, search_bar);
	}
}

int
nld_search_bar_get_search_timeout (NldSearchBar * search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	return priv->search_timeout;
}

void
nld_search_bar_set_search_timeout (NldSearchBar * search_bar, int search_timeout)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	if (priv->search_timeout != -1 && search_timeout == -1)
		g_signal_handlers_disconnect_by_func (priv->entry, entry_changed, search_bar);
	else if (search_timeout != -1)
	{
		g_signal_connect (priv->entry, "changed", G_CALLBACK (entry_changed), search_bar);
	}

	priv->search_timeout = search_timeout;
}

const char *
nld_search_bar_get_text (NldSearchBar * search_bar)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	return ctk_entry_get_text (priv->entry);
}

void
nld_search_bar_set_text (NldSearchBar * search_bar, const char *text, gboolean activate)
{
	NldSearchBarPrivate *priv = nld_search_bar_get_instance_private (search_bar);

	ctk_entry_set_text (priv->entry, text);
	if (activate)
		emit_search (search_bar);
}

