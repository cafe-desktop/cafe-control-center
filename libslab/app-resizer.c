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

#include <ctk/ctk.h>

#include "app-shell.h"
#include "app-resizer.h"

static void app_resizer_size_allocate (CtkWidget * resizer, CtkAllocation * allocation);
static gboolean app_resizer_paint_window (CtkWidget * widget, cairo_t * cr, AppShellData * app_data);

G_DEFINE_TYPE (AppResizer, app_resizer, CTK_TYPE_LAYOUT);


static void
app_resizer_class_init (AppResizerClass * klass)
{
	CtkWidgetClass *widget_class;

	widget_class = CTK_WIDGET_CLASS (klass);
	widget_class->size_allocate = app_resizer_size_allocate;
}

static void
app_resizer_init (AppResizer * window)
{
    ctk_style_context_add_class (ctk_widget_get_style_context (CTK_WIDGET (window)),
                                 CTK_STYLE_CLASS_VIEW);
}

void
remove_container_entries (CtkContainer * widget)
{
	GList *children, *l;

	children = ctk_container_get_children (widget);
	for (l = children; l; l = l->next)
	{
		CtkWidget *child = CTK_WIDGET (l->data);
		ctk_container_remove (CTK_CONTAINER (widget), CTK_WIDGET (child));
	}

	if (children)
		g_list_free (children);
}

static void
resize_table (CtkTable * table, gint columns, GList * launcher_list)
{
	float rows, remainder;

	remove_container_entries (CTK_CONTAINER (table));

	rows = ((float) g_list_length (launcher_list)) / (float) columns;
	remainder = rows - ((int) rows);
	if (remainder != 0.0)
		rows += 1;

	ctk_table_resize (table, (int) rows, columns);
}

static void
relayout_table (CtkTable * table, GList * element_list)
{
	guint maxcols, maxrows;
	ctk_table_get_size (CTK_TABLE (table), &maxrows, &maxcols);
	gint row = 0, col = 0;
	do
	{
		CtkWidget *element = CTK_WIDGET (element_list->data);
		ctk_table_attach (table, element, col, col + 1, row, row + 1, CTK_EXPAND | CTK_FILL,
			CTK_EXPAND | CTK_FILL, 0, 0);
		col++;
		if (col == maxcols)
		{
			col = 0;
			row++;
		}
	}
	while (NULL != (element_list = g_list_next (element_list)));
}

void
app_resizer_layout_table_default (AppResizer * widget, CtkTable * table, GList * element_list)
{
	resize_table (table, widget->cur_num_cols, element_list);
	relayout_table (table, element_list);
}

static void
relayout_tables (AppResizer * widget, gint num_cols)
{
	CtkTable *table;
	GList *table_list, *launcher_list;

	for (table_list = widget->cached_tables_list; table_list != NULL;
		table_list = g_list_next (table_list))
	{
		table = CTK_TABLE (table_list->data);
		launcher_list = ctk_container_get_children (CTK_CONTAINER (table));
		launcher_list = g_list_reverse (launcher_list);	/* Fixme - ugly hack because table stores prepend */
		resize_table (table, num_cols, launcher_list);
		relayout_table (table, launcher_list);
		g_list_free (launcher_list);
	}
}

static gint
calculate_num_cols (AppResizer * resizer, gint avail_width)
{
	if (resizer->table_elements_homogeneous)
	{
		gint num_cols;

		if (resizer->cached_element_width == -1)
		{
			CtkTable *table = CTK_TABLE (resizer->cached_tables_list->data);
			GList *children = ctk_container_get_children (CTK_CONTAINER (table));
			CtkWidget *table_element = CTK_WIDGET (children->data);
			gint natural_width;
			g_list_free (children);

			ctk_widget_get_preferred_width (table_element, NULL, &natural_width);
			resizer->cached_element_width = natural_width;
			resizer->cached_table_spacing = ctk_table_get_default_col_spacing (table);
		}

		num_cols =
			(avail_width +
			resizer->cached_table_spacing) / (resizer->cached_element_width +
			resizer->cached_table_spacing);
		return num_cols;
	}
	else
		g_assert_not_reached ();	/* Fixme - implement... */
}

static gint
relayout_tables_if_needed (AppResizer * widget, gint avail_width, gint current_num_cols)
{
	gint num_cols = calculate_num_cols (widget, avail_width);
	if (num_cols < 1)
	{
		num_cols = 1;	/* just horiz scroll if avail_width is less than one column */
	}

	if (current_num_cols != num_cols)
	{
		relayout_tables (widget, num_cols);
		current_num_cols = num_cols;
	}
	return current_num_cols;
}

void
app_resizer_set_table_cache (AppResizer * widget, GList * cache_list)
{
	widget->cached_tables_list = cache_list;
}

static void
app_resizer_size_allocate (CtkWidget * widget, CtkAllocation * allocation)
{
	AppResizer *resizer = APP_RESIZER (widget);
	CtkWidget *child = CTK_WIDGET (APP_RESIZER (resizer)->child);
	CtkAllocation widget_allocation;
	CtkRequisition child_requisition;

	static gboolean first_time = TRUE;
	gint new_num_cols;
	gint useable_area;

	if (first_time)
	{
		/* we are letting the first show be the "natural" size of the child widget so do nothing. */
		if (CTK_WIDGET_CLASS (app_resizer_parent_class)->size_allocate)
			(*CTK_WIDGET_CLASS (app_resizer_parent_class)->size_allocate) (widget, allocation);

		first_time = FALSE;
		ctk_widget_get_allocation (child, &widget_allocation);
		ctk_layout_set_size (CTK_LAYOUT (resizer), widget_allocation.width,
			widget_allocation.height);
		return;
	}

	ctk_widget_get_preferred_size (child, &child_requisition, NULL);

	if (!resizer->cached_tables_list)	/* if everthing is currently filtered out - just return */
	{
		CtkAllocation child_allocation;

		if (CTK_WIDGET_CLASS (app_resizer_parent_class)->size_allocate)
			(*CTK_WIDGET_CLASS (app_resizer_parent_class)->size_allocate) (widget, allocation);

		/* We want the message to center itself and only scroll if it's bigger than the available real size. */
		child_allocation.x = 0;
		child_allocation.y = 0;
		child_allocation.width = MAX (allocation->width, child_requisition.width);
		child_allocation.height = MAX (allocation->height, child_requisition.height);

		ctk_widget_size_allocate (child, &child_allocation);
		ctk_layout_set_size (CTK_LAYOUT (resizer), child_allocation.width,
			child_allocation.height);
		return;
	}
	CtkRequisition other_requisiton;
	ctk_widget_get_preferred_size (CTK_WIDGET (resizer->cached_tables_list->data), &other_requisiton, NULL);

	useable_area =
		allocation->width - (child_requisition.width -
		other_requisiton.width);
	new_num_cols =
		relayout_tables_if_needed (APP_RESIZER (resizer), useable_area,
		resizer->cur_num_cols);
	if (resizer->cur_num_cols != new_num_cols)
	{
		CtkRequisition req;

		/* Have to do this so that it requests, and thus gets allocated, new amount */
		ctk_widget_get_preferred_size (child, &req, NULL);

		resizer->cur_num_cols = new_num_cols;
	}

	if (CTK_WIDGET_CLASS (app_resizer_parent_class)->size_allocate)
		(*CTK_WIDGET_CLASS (app_resizer_parent_class)->size_allocate) (widget, allocation);
	ctk_widget_get_allocation (child, &widget_allocation);
	ctk_layout_set_size (CTK_LAYOUT (resizer), widget_allocation.width,
		widget_allocation.height);
}

CtkWidget *
app_resizer_new (CtkBox * child, gint initial_num_columns, gboolean homogeneous,
	AppShellData * app_data)
{
	AppResizer *widget;

	g_assert (child != NULL);

	widget = g_object_new (APP_RESIZER_TYPE, NULL);
	widget->cached_element_width = -1;
	widget->cur_num_cols = initial_num_columns;
	widget->table_elements_homogeneous = homogeneous;
	widget->setting_style = FALSE;
	widget->app_data = app_data;

	g_signal_connect (G_OBJECT (widget), "draw", G_CALLBACK (app_resizer_paint_window), app_data);

	ctk_container_add (CTK_CONTAINER (widget), CTK_WIDGET (child));
	widget->child = child;

	return CTK_WIDGET (widget);
}

void
app_resizer_set_vadjustment_value (CtkWidget * widget, gdouble value)
{
	CtkAdjustment *adjust;

	adjust = ctk_scrollable_get_vadjustment (CTK_SCROLLABLE (widget));

	gdouble upper = ctk_adjustment_get_upper (adjust);
	gdouble page_size = ctk_adjustment_get_page_size (adjust);
	if (value > upper - page_size)
	{
		value = upper - page_size;
	}
	ctk_adjustment_set_value (adjust, value);
}

static gboolean
app_resizer_paint_window (CtkWidget * widget, cairo_t * cr, AppShellData * app_data)
{
	cairo_save(cr);

	CtkAllocation widget_allocation;
	ctk_widget_get_allocation (widget, &widget_allocation);

	gdk_cairo_set_source_color (cr, ctk_widget_get_style (widget)->base);
	cairo_set_line_width(cr, 1);

	cairo_rectangle(cr, widget_allocation.x, widget_allocation.y, widget_allocation.width, widget_allocation.height);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);

	if (app_data->selected_group)
	{
		CtkWidget *selected_widget = CTK_WIDGET (app_data->selected_group);
		CtkAllocation selected_widget_allocation;

		ctk_widget_get_allocation (selected_widget, &selected_widget_allocation);

		gdk_cairo_set_source_color (cr, ctk_widget_get_style (selected_widget)->light);
		cairo_set_line_width(cr, 1);

		cairo_rectangle(cr, selected_widget_allocation.x, selected_widget_allocation.y, selected_widget_allocation.width, selected_widget_allocation.height);
		cairo_stroke_preserve(cr);
		cairo_fill(cr);
	}

	cairo_restore(cr);

	return FALSE;
}
