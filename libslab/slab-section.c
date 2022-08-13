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

#include "slab-section.h"

G_DEFINE_TYPE (SlabSection, slab_section, CTK_TYPE_BOX)

static void slab_section_finalize (GObject *);

static void slab_section_class_init (SlabSectionClass * slab_section_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (slab_section_class);

	g_obj_class->finalize = slab_section_finalize;
}

static void
slab_section_init (SlabSection * section)
{
	section->title = NULL;
	section->contents = NULL;
}

static void
slab_section_finalize (GObject * obj)
{
	g_assert (IS_SLAB_SECTION (obj));
	(*G_OBJECT_CLASS (slab_section_parent_class)->finalize) (obj);
}

static void
slab_section_set_title_color (CtkWidget * widget)
{
	switch (SLAB_SECTION (widget)->style)
	{
	case Style1:
		ctk_widget_modify_fg (SLAB_SECTION (widget)->title, CTK_STATE_NORMAL,
			&ctk_widget_get_style (widget)->bg[CTK_STATE_SELECTED]);
		break;
	case Style2:
		if (SLAB_SECTION (widget)->selected)
			ctk_widget_modify_fg (SLAB_SECTION (widget)->title, CTK_STATE_NORMAL,
				&ctk_widget_get_style (widget)->dark[CTK_STATE_SELECTED]);
		else
			ctk_widget_modify_fg (SLAB_SECTION (widget)->title, CTK_STATE_NORMAL,
				&ctk_widget_get_style (widget)->text[CTK_STATE_INSENSITIVE]);
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
slab_section_style_set (CtkWidget * widget, CtkStyle * prev_style, gpointer user_data)
{
	static gboolean recursively_entered = FALSE;
	if (!recursively_entered)
	{
		recursively_entered = TRUE;

		slab_section_set_title_color (widget);

		recursively_entered = FALSE;
	}
}

/*
gboolean
slab_section_expose_event (CtkWidget * widget, CdkEventExpose * event, gpointer data)
{
	cdk_draw_rectangle (widget->window, widget->style->light_gc[CTK_STATE_SELECTED], TRUE,
		widget->allocation.x, widget->allocation.y,
		widget->allocation.width + 40, widget->allocation.height);

	return FALSE;
}
*/

void
slab_section_set_selected (SlabSection * section, gboolean selected)
{
	if (selected == section->selected)
		return;
	section->selected = selected;

	/*
	   if(selected)
	   {
	   section->expose_handler_id = g_signal_connect(G_OBJECT(section),
	   "expose-event", G_CALLBACK(slab_section_expose_event), NULL);
	   }
	   else
	   {
	   g_signal_handler_disconnect(section, section->expose_handler_id);
	   }
	 */

	slab_section_set_title_color (CTK_WIDGET (section));
}

CtkWidget *
slab_section_new_with_markup (const gchar * title_markup, SlabStyle style)
{
	SlabSection *section;
	gchar * widget_theming_name;

	section = g_object_new (SLAB_SECTION_TYPE, NULL);
	ctk_orientable_set_orientation (CTK_ORIENTABLE (section), CTK_ORIENTATION_VERTICAL);
	ctk_box_set_homogeneous (CTK_BOX (section), FALSE);
	ctk_box_set_spacing (CTK_BOX (section), 0);
	section->style = style;
	section->selected = FALSE;

	section->childbox = CTK_BOX (ctk_box_new (CTK_ORIENTATION_VERTICAL, 10));
	switch (style)
	{
	case Style1:
		widget_theming_name = "slab_section_style1";
		break;
	case Style2:
		ctk_widget_set_margin_top (CTK_WIDGET (section->childbox), SLAB_TOP_PADDING);
		ctk_widget_set_margin_bottom (CTK_WIDGET (section->childbox), SLAB_BOTTOM_PADDING);
		ctk_widget_set_margin_start (CTK_WIDGET (section->childbox), SLAB_LEFT_PADDING);
		ctk_widget_set_margin_end (CTK_WIDGET (section->childbox), 0);
		widget_theming_name = "slab_section_style2";
		break;
	default:
		g_assert_not_reached ();
	}
	ctk_box_pack_start (CTK_BOX (section), CTK_WIDGET (section->childbox), TRUE, TRUE, 0);

	section->title = ctk_label_new (title_markup);
	ctk_label_set_use_markup (CTK_LABEL (section->title), TRUE);
	ctk_label_set_xalign (CTK_LABEL (section->title), 0.0);

	ctk_widget_set_name (CTK_WIDGET (section), widget_theming_name);
	g_signal_connect (G_OBJECT (section), "style-set", G_CALLBACK (slab_section_style_set),
		NULL);

	ctk_box_pack_start (section->childbox, section->title, FALSE, FALSE, 0);

	return CTK_WIDGET (section);
}

CtkWidget *
slab_section_new (const gchar * title, SlabStyle style)
{
	CtkWidget *section;
	gchar *markup;

	markup = g_strdup_printf ("<span size=\"large\" weight=\"bold\">%s</span>", title);
	section = slab_section_new_with_markup (markup, style);

	g_free (markup);

	return section;
}

void
slab_section_set_title (SlabSection * section, const gchar * title)
{
	gchar *markup = g_strdup_printf ("<span size=\"large\">%s</span>", title);

	ctk_label_set_markup (CTK_LABEL (section->title), markup);

	g_free (markup);
}

void
slab_section_set_contents (SlabSection * section, CtkWidget * contents)
{
	section->contents = contents;

	ctk_box_pack_start (section->childbox, contents, FALSE, FALSE, 0);
}
