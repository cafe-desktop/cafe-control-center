/* -*- mode: c; style: linux -*- */

/* cafe-keyboard-properties-xkbmc.c
 * Copyright (C) 2003-2007 Sergey V. Udaltsov
 *
 * Written by: Sergey V. Udaltsov <svu@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <ctk/ctk.h>

#include "capplet-util.h"

#include "cafe-keyboard-properties-xkb.h"

static gchar *current_model_name = NULL;
static gchar *current_vendor_name = NULL;

static void fill_models_list (CtkBuilder * chooser_dialog);

static gboolean fill_vendors_list (CtkBuilder * chooser_dialog);

static CtkTreePath *
ctk_list_store_find_entry (CtkListStore * list_store,
			   CtkTreeIter * iter, gchar * name, int column_id)
{
	CtkTreePath *path;
	char *current_name = NULL;
	if (ctk_tree_model_get_iter_first
	    (CTK_TREE_MODEL (list_store), iter)) {
		do {
			ctk_tree_model_get (CTK_TREE_MODEL
					    (list_store), iter, column_id,
					    &current_name, -1);
			if (!g_ascii_strcasecmp (name, current_name)) {
				path =
				    ctk_tree_model_get_path
				    (CTK_TREE_MODEL (list_store), iter);
				return path;
			}
			g_free (current_name);
		} while (ctk_tree_model_iter_next
			 (CTK_TREE_MODEL (list_store), iter));
	}
	return NULL;
}

static void
add_vendor_to_list (XklConfigRegistry * config_registry,
		    XklConfigItem * config_item,
		    CtkTreeView * vendors_list)
{
	CtkTreeIter iter;
	CtkTreePath *found_existing;
	CtkListStore *list_store;

	gchar *vendor_name =
	    (gchar *) g_object_get_data (G_OBJECT (config_item),
					 XCI_PROP_VENDOR);

	if (vendor_name == NULL)
		return;

	list_store =
	    CTK_LIST_STORE (ctk_tree_view_get_model (vendors_list));

	if (!g_ascii_strcasecmp (config_item->name, current_model_name)) {
		current_vendor_name = g_strdup (vendor_name);
	}

	found_existing =
	    ctk_list_store_find_entry (list_store, &iter, vendor_name, 0);
	/* This vendor is already there */
	if (found_existing != NULL) {
		ctk_tree_path_free (found_existing);
		return;
	}

	ctk_list_store_append (list_store, &iter);
	ctk_list_store_set (list_store, &iter, 0, vendor_name, -1);
}

static void
add_model_to_list (XklConfigRegistry * config_registry,
		   XklConfigItem * config_item, CtkTreeView * models_list)
{
	CtkTreeIter iter;
	CtkListStore *list_store =
	    CTK_LIST_STORE (ctk_tree_view_get_model (models_list));
	char *utf_model_name;
	if (current_vendor_name != NULL) {
		gchar *vendor_name =
		    (gchar *) g_object_get_data (G_OBJECT (config_item),
						 XCI_PROP_VENDOR);
		if (vendor_name == NULL)
			return;

		if (g_ascii_strcasecmp (vendor_name, current_vendor_name))
			return;
	}
	utf_model_name = xci_desc_to_utf8 (config_item);
	ctk_list_store_append (list_store, &iter);
	ctk_list_store_set (list_store, &iter,
			    0, utf_model_name, 1, config_item->name, -1);

	g_free (utf_model_name);
}

static void
xkb_model_chooser_change_vendor_sel (CtkTreeSelection * selection,
				     CtkBuilder * chooser_dialog)
{
	CtkTreeIter iter;
	CtkTreeModel *list_store = NULL;
	if (ctk_tree_selection_get_selected
	    (selection, &list_store, &iter)) {
		gchar *vendor_name = NULL;
		ctk_tree_model_get (list_store, &iter,
				    0, &vendor_name, -1);

		current_vendor_name = vendor_name;
		fill_models_list (chooser_dialog);
		g_free (vendor_name);
	} else {
		current_vendor_name = NULL;
		fill_models_list (chooser_dialog);
	}

}

static void
xkb_model_chooser_change_model_sel (CtkTreeSelection * selection,
				    CtkBuilder * chooser_dialog)
{
	gboolean anysel =
	    ctk_tree_selection_get_selected (selection, NULL, NULL);
	ctk_dialog_set_response_sensitive (CTK_DIALOG
					   (CWID ("xkb_model_chooser")),
					   CTK_RESPONSE_OK, anysel);
}

static void
prepare_vendors_list (CtkBuilder * chooser_dialog)
{
	CtkWidget *vendors_list = CWID ("vendors_list");
	CtkCellRenderer *renderer = ctk_cell_renderer_text_new ();
	CtkTreeViewColumn *vendor_col =
	    ctk_tree_view_column_new_with_attributes (_("Vendors"),
						      renderer,
						      "text", 0,
						      NULL);
	ctk_tree_view_column_set_visible (vendor_col, TRUE);
	ctk_tree_view_append_column (CTK_TREE_VIEW (vendors_list),
				     vendor_col);
}

static gboolean
fill_vendors_list (CtkBuilder * chooser_dialog)
{
	CtkWidget *vendors_list = CWID ("vendors_list");
	CtkListStore *list_store = ctk_list_store_new (1, G_TYPE_STRING);
	CtkTreeIter iter;
	CtkTreePath *path;

	ctk_tree_view_set_model (CTK_TREE_VIEW (vendors_list),
				 CTK_TREE_MODEL (list_store));

	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE
					      (list_store), 0,
					      CTK_SORT_ASCENDING);

	current_vendor_name = NULL;

	xkl_config_registry_foreach_model (config_registry,
					   (ConfigItemProcessFunc)
					   add_vendor_to_list,
					   vendors_list);

	if (current_vendor_name != NULL) {
		path = ctk_list_store_find_entry (list_store,
						  &iter,
						  current_vendor_name, 0);
		if (path != NULL) {
			ctk_tree_selection_select_iter
			    (ctk_tree_view_get_selection
			     (CTK_TREE_VIEW (vendors_list)), &iter);
			ctk_tree_view_scroll_to_cell
			    (CTK_TREE_VIEW (vendors_list),
			     path, NULL, TRUE, 0.5, 0);
			ctk_tree_path_free (path);
		}
		fill_models_list (chooser_dialog);
		g_free (current_vendor_name);
	} else {
		fill_models_list (chooser_dialog);
	}

	g_signal_connect (G_OBJECT
			  (ctk_tree_view_get_selection
			   (CTK_TREE_VIEW (vendors_list))), "changed",
			  G_CALLBACK (xkb_model_chooser_change_vendor_sel),
			  chooser_dialog);

	return ctk_tree_model_get_iter_first (CTK_TREE_MODEL (list_store),
					      &iter);
}

static void
prepare_models_list (CtkBuilder * chooser_dialog)
{
	CtkWidget *models_list = CWID ("models_list");
	CtkCellRenderer *renderer = ctk_cell_renderer_text_new ();
	CtkTreeViewColumn *description_col =
	    ctk_tree_view_column_new_with_attributes (_("Models"),
						      renderer,
						      "text", 0,
						      NULL);
	ctk_tree_view_column_set_visible (description_col, TRUE);
	ctk_tree_view_append_column (CTK_TREE_VIEW (models_list),
				     description_col);
}

static void
fill_models_list (CtkBuilder * chooser_dialog)
{
	CtkWidget *models_list = CWID ("models_list");
	CtkTreeIter iter;
	CtkTreePath *path;

	CtkListStore *list_store =
	    ctk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE
					      (list_store), 0,
					      CTK_SORT_ASCENDING);

	ctk_tree_view_set_model (CTK_TREE_VIEW (models_list),
				 CTK_TREE_MODEL (list_store));

	xkl_config_registry_foreach_model (config_registry,
					   (ConfigItemProcessFunc)
					   add_model_to_list, models_list);

	if (current_model_name != NULL) {
		path = ctk_list_store_find_entry (list_store,
						  &iter,
						  current_model_name, 1);
		if (path != NULL) {
			ctk_tree_selection_select_iter
			    (ctk_tree_view_get_selection
			     (CTK_TREE_VIEW (models_list)), &iter);
			ctk_tree_view_scroll_to_cell
			    (CTK_TREE_VIEW (models_list),
			     path, NULL, TRUE, 0.5, 0);
			ctk_tree_path_free (path);
		}
	}

	g_signal_connect (G_OBJECT
			  (ctk_tree_view_get_selection
			   (CTK_TREE_VIEW (models_list))), "changed",
			  G_CALLBACK (xkb_model_chooser_change_model_sel),
			  chooser_dialog);
}

static void
xkb_model_chooser_response (CtkDialog * dialog,
			    gint response, CtkBuilder * chooser_dialog)
{
	if (response == CTK_RESPONSE_OK) {
		CtkWidget *models_list = CWID ("models_list");
		CtkTreeSelection *selection =
		    ctk_tree_view_get_selection (CTK_TREE_VIEW
						 (models_list));
		CtkTreeIter iter;
		CtkTreeModel *list_store = NULL;
		if (ctk_tree_selection_get_selected
		    (selection, &list_store, &iter)) {
			gchar *model_name = NULL;
			ctk_tree_model_get (list_store, &iter,
					    1, &model_name, -1);

			g_settings_set_string (xkb_kbd_settings, "model", model_name);
			g_free (model_name);
		}
	}
}

void
choose_model (CtkBuilder * dialog)
{
	CtkBuilder *chooser_dialog;
	CtkWidget *chooser;

	chooser_dialog = ctk_builder_new ();
	ctk_builder_add_from_resource (chooser_dialog,
	                               "/org/cafe/mcc/keyboard/cafe-keyboard-properties-model-chooser.ui",
	                               NULL);
	chooser = CWID ("xkb_model_chooser");
	ctk_window_set_transient_for (CTK_WINDOW (chooser),
				      CTK_WINDOW (WID
						  ("keyboard_dialog")));
	current_model_name =
	    g_settings_get_string (xkb_kbd_settings, "model");

	prepare_vendors_list (chooser_dialog);
	prepare_models_list (chooser_dialog);

	if (!fill_vendors_list (chooser_dialog)) {
		ctk_widget_hide (CWID ("vendors_label"));
		ctk_widget_hide (CWID ("vendors_scrolledwindow"));
		current_vendor_name = NULL;
		fill_models_list (chooser_dialog);
	}

	g_signal_connect (G_OBJECT (chooser),
			  "response",
			  G_CALLBACK (xkb_model_chooser_response),
			  chooser_dialog);
	ctk_dialog_run (CTK_DIALOG (chooser));
	ctk_widget_destroy (chooser);
	g_free (current_model_name);
}
