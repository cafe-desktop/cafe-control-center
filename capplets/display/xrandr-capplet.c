/* Monitor Settings. A preference panel for configuring monitors
 *
 * Copyright (C) 2007, 2008  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <ctk/ctk.h>
#include "scrollarea.h"
#define CAFE_DESKTOP_USE_UNSTABLE_API
#include <libcafe-desktop/cafe-desktop-utils.h>
#include <libcafe-desktop/cafe-rr.h>
#include <libcafe-desktop/cafe-rr-config.h>
#include <libcafe-desktop/cafe-rr-labeler.h>
#include <cdk/cdkx.h>
#include <X11/Xlib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "capplet-util.h"

typedef struct App App;
typedef struct GrabInfo GrabInfo;

struct App
{
    CafeRRScreen       *screen;
    CafeRRConfig  *current_configuration;
    CafeRRLabeler *labeler;
    CafeRROutputInfo         *current_output;

    CtkWidget	   *dialog;
    CtkWidget      *current_monitor_event_box;
    CtkWidget      *current_monitor_label;
    CtkWidget      *monitor_on_radio;
    CtkWidget      *monitor_off_radio;
    CtkWidget	   *resolution_combo;
    CtkWidget	   *refresh_combo;
    CtkWidget	   *rotation_combo;
    CtkWidget	   *panel_checkbox;
    CtkWidget	   *clone_checkbox;
    CtkWidget	   *show_icon_checkbox;
    CtkWidget      *primary_button;

    /* We store the event timestamp when the Apply button is clicked */
    CtkWidget      *apply_button;
    guint32         apply_button_clicked_timestamp;

    CtkWidget      *area;
    gboolean	    ignore_gui_changes;
    GSettings	   *settings;

    /* These are used while we are waiting for the ApplyConfiguration method to be executed over D-bus */
    GDBusConnection *connection;
    GDBusProxy *proxy;

    enum {
	APPLYING_VERSION_1,
	APPLYING_VERSION_2
    } apply_configuration_state;
};

/* Response codes for custom buttons in the main dialog */
enum {
    RESPONSE_MAKE_DEFAULT = 1
};

static void rebuild_gui (App *app);
static void on_clone_changed (CtkWidget *box, gpointer data);
static void on_rate_changed (CtkComboBox *box, gpointer data);
static gboolean output_overlaps (CafeRROutputInfo *output, CafeRRConfig *config);
static void select_current_output_from_dialog_position (App *app);
static void monitor_on_off_toggled_cb (CtkToggleButton *toggle, gpointer data);
static void get_geometry (CafeRROutputInfo *output, int *w, int *h);
static void apply_configuration_returned_cb (GObject *source_object, GAsyncResult *res, gpointer data);
static gboolean get_clone_size (CafeRRScreen *screen, int *width, int *height);
static gboolean output_info_supports_mode (App *app, CafeRROutputInfo *info, int width, int height);

static void
error_message (App *app, const char *primary_text, const char *secondary_text)
{
    CtkWidget *dialog;

    dialog = ctk_message_dialog_new ((app && app->dialog) ? CTK_WINDOW (app->dialog) : NULL,
				     CTK_DIALOG_MODAL | CTK_DIALOG_DESTROY_WITH_PARENT,
				     CTK_MESSAGE_ERROR,
				     CTK_BUTTONS_CLOSE,
				     "%s", primary_text);

    if (secondary_text)
	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog), "%s", secondary_text);

    ctk_dialog_run (CTK_DIALOG (dialog));
    ctk_widget_destroy (dialog);
}

static gboolean
do_free (gpointer data)
{
    g_free (data);
    return FALSE;
}

static gchar *
idle_free (gchar *s)
{
    g_idle_add (do_free, s);

    return s;
}

static void
on_screen_changed (CafeRRScreen *scr,
		   gpointer data)
{
    CafeRRConfig *current;
    App *app = data;

    current = cafe_rr_config_new_current (app->screen, NULL);

    if (app->current_configuration)
	g_object_unref (app->current_configuration);

    app->current_configuration = current;
    app->current_output = NULL;

    if (app->labeler) {
	cafe_rr_labeler_hide (app->labeler);
	g_object_unref (app->labeler);
    }

    app->labeler = cafe_rr_labeler_new (app->current_configuration);

    select_current_output_from_dialog_position (app);
}

static void
on_viewport_changed (FooScrollArea *scroll_area,
		     CdkRectangle  *old_viewport,
		     CdkRectangle  *new_viewport)
{
    foo_scroll_area_set_size (scroll_area,
			      new_viewport->width,
			      new_viewport->height);

    foo_scroll_area_invalidate (scroll_area);
}

static void
layout_set_font (PangoLayout *layout, const char *font)
{
    PangoFontDescription *desc =
	pango_font_description_from_string (font);

    if (desc)
    {
	pango_layout_set_font_description (layout, desc);

	pango_font_description_free (desc);
    }
}

static void
clear_combo (CtkWidget *widget)
{
    CtkComboBox *box = CTK_COMBO_BOX (widget);
    CtkTreeModel *model = ctk_combo_box_get_model (box);
    CtkListStore *store = CTK_LIST_STORE (model);

    ctk_list_store_clear (store);
}

typedef struct
{
    const char *text;
    gboolean found;
    CtkTreeIter iter;
} ForeachInfo;

static gboolean
foreach (CtkTreeModel *model,
	 CtkTreePath *path,
	 CtkTreeIter *iter,
	 gpointer data)
{
    ForeachInfo *info = data;
    char *text = NULL;

    ctk_tree_model_get (model, iter, 0, &text, -1);

    g_assert (text != NULL);

    if (strcmp (info->text, text) == 0)
    {
	info->found = TRUE;
	info->iter = *iter;
	return TRUE;
    }

    return FALSE;
}

static void
add_key (CtkWidget *widget,
	 const char *text,
	 int width, int height, int rate,
	 CafeRRRotation rotation)
{
    ForeachInfo info;
    CtkComboBox *box = CTK_COMBO_BOX (widget);
    CtkTreeModel *model = ctk_combo_box_get_model (box);
    CtkListStore *store = CTK_LIST_STORE (model);

    info.text = text;
    info.found = FALSE;

    ctk_tree_model_foreach (model, foreach, &info);

    if (!info.found)
    {
	CtkTreeIter iter;
	ctk_list_store_insert_with_values (store, &iter, -1,
                                           0, text,
                                           1, width,
                                           2, height,
                                           3, rate,
                                           4, width * height,
                                           5, rotation,
                                           -1);

    }
}

static gboolean
combo_select (CtkWidget *widget, const char *text)
{
    CtkComboBox *box = CTK_COMBO_BOX (widget);
    CtkTreeModel *model = ctk_combo_box_get_model (box);
    ForeachInfo info;

    info.text = text;
    info.found = FALSE;

    ctk_tree_model_foreach (model, foreach, &info);

    if (!info.found)
	return FALSE;

    ctk_combo_box_set_active_iter (box, &info.iter);
    return TRUE;
}

static CafeRRMode **
get_current_modes (App *app)
{
    CafeRROutput *output;

    if (cafe_rr_config_get_clone (app->current_configuration))
    {
	return cafe_rr_screen_list_clone_modes (app->screen);
    }
    else
    {
	if (!app->current_output)
	    return NULL;

	output = cafe_rr_screen_get_output_by_name (app->screen,
	    cafe_rr_output_info_get_name (app->current_output));

	if (!output)
	    return NULL;

	return cafe_rr_output_list_modes (output);
    }
}

static void
rebuild_rotation_combo (App *app)
{
    typedef struct
    {
	CafeRRRotation	rotation;
	const char *	name;
    } RotationInfo;
    static const RotationInfo rotations[] = {
	{ CAFE_RR_ROTATION_0, N_("Normal") },
	{ CAFE_RR_ROTATION_90, N_("Left") },
	{ CAFE_RR_ROTATION_270, N_("Right") },
	{ CAFE_RR_ROTATION_180, N_("Upside Down") },
    };
    const char *selection;
    CafeRRRotation current;
    int i;

    clear_combo (app->rotation_combo);

    ctk_widget_set_sensitive (app->rotation_combo,
                              app->current_output && cafe_rr_output_info_is_active (app->current_output));

    if (!app->current_output)
	return;

    current = cafe_rr_output_info_get_rotation (app->current_output);

    selection = NULL;
    for (i = 0; i < G_N_ELEMENTS (rotations); ++i)
    {
	const RotationInfo *info = &(rotations[i]);

	cafe_rr_output_info_set_rotation (app->current_output, info->rotation);

	/* NULL-GError --- FIXME: we should say why this rotation is not available! */
	if (cafe_rr_config_applicable (app->current_configuration, app->screen, NULL))
	{
 	    add_key (app->rotation_combo, _(info->name), 0, 0, 0, info->rotation);

	    if (info->rotation == current)
		selection = _(info->name);
	}
    }

    cafe_rr_output_info_set_rotation (app->current_output, current);

    if (!(selection && combo_select (app->rotation_combo, selection)))
	combo_select (app->rotation_combo, _("Normal"));
}

static char *
make_rate_string (int hz)
{
    return g_strdup_printf (_("%d Hz"), hz);
}

static void
rebuild_rate_combo (App *app)
{
    CafeRRMode **modes;
    int best;
    int i;

    clear_combo (app->refresh_combo);

    ctk_widget_set_sensitive (
	app->refresh_combo, app->current_output && cafe_rr_output_info_is_active (app->current_output));

    if (!app->current_output
        || !(modes = get_current_modes (app)))
	return;

    best = -1;
    for (i = 0; modes[i] != NULL; ++i)
    {
	CafeRRMode *mode = modes[i];
	int width, height, rate;
	int output_width, output_height;

	cafe_rr_output_info_get_geometry (app->current_output, NULL, NULL, &output_width, &output_height);

	width = cafe_rr_mode_get_width (mode);
	height = cafe_rr_mode_get_height (mode);
	rate = cafe_rr_mode_get_freq (mode);

	if (width == output_width		&&
	    height == output_height)
	{
	    add_key (app->refresh_combo,
		     idle_free (make_rate_string (rate)),
		     0, 0, rate, -1);

	    if (rate > best)
		best = rate;
	}
    }

    if (!combo_select (app->refresh_combo, idle_free (make_rate_string (cafe_rr_output_info_get_refresh_rate (app->current_output)))))
	combo_select (app->refresh_combo, idle_free (make_rate_string (best)));
}

static int
count_active_outputs (App *app)
{
    int i, count = 0;
    CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (app->current_configuration);

    for (i = 0; outputs[i] != NULL; ++i)
    {
	if (cafe_rr_output_info_is_active (outputs[i]))
	    count++;
    }

    return count;
}

/* Computes whether "Mirror Screens" (clone mode) is supported based on these criteria:
 *
 * 1. There is an available size for cloning.
 *
 * 2. There are 2 or more connected outputs that support that size.
 */
static gboolean
mirror_screens_is_supported (App *app)
{
    int clone_width, clone_height;
    gboolean have_clone_size;
    gboolean mirror_is_supported;

    mirror_is_supported = FALSE;

    have_clone_size = get_clone_size (app->screen, &clone_width, &clone_height);

    if (have_clone_size) {
	int i;
	int num_outputs_with_clone_size;
	CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (app->current_configuration);

	num_outputs_with_clone_size = 0;

	for (i = 0; outputs[i] != NULL; i++)
	{
	    /* We count the connected outputs that support the clone size.  It
	     * doesn't matter if those outputs aren't actually On currently; we
	     * will turn them on in on_clone_changed().
	     */
	    if (cafe_rr_output_info_is_connected (outputs[i]) && output_info_supports_mode (app, outputs[i], clone_width, clone_height))
		num_outputs_with_clone_size++;
	}

	if (num_outputs_with_clone_size >= 2)
	    mirror_is_supported = TRUE;
    }

    return mirror_is_supported;
}

static void
rebuild_mirror_screens (App *app)
{
    gboolean mirror_is_active;
    gboolean mirror_is_supported;

    g_signal_handlers_block_by_func (app->clone_checkbox, G_CALLBACK (on_clone_changed), app);

    mirror_is_active = app->current_configuration && cafe_rr_config_get_clone (app->current_configuration);

    /* If mirror_is_active, then it *must* be possible to turn mirroring off */
    mirror_is_supported = mirror_is_active || mirror_screens_is_supported (app);

    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (app->clone_checkbox), mirror_is_active);
    ctk_widget_set_sensitive (app->clone_checkbox, mirror_is_supported);

    g_signal_handlers_unblock_by_func (app->clone_checkbox, G_CALLBACK (on_clone_changed), app);
}

static void
rebuild_current_monitor_label (App *app)
{
	char *str, *tmp;
	CdkRGBA color;
	gboolean use_color;

	if (app->current_output)
	{
	    if (cafe_rr_config_get_clone (app->current_configuration))
		tmp = g_strdup (_("Mirror Screens"));
	    else
		tmp = g_strdup_printf (_("Monitor: %s"), cafe_rr_output_info_get_display_name (app->current_output));

	    str = g_strdup_printf ("<b>%s</b>", tmp);
	    cafe_rr_labeler_get_rgba_for_output (app->labeler, app->current_output, &color);
	    use_color = TRUE;
	    g_free (tmp);
	}
	else
	{
	    str = g_strdup_printf ("<b>%s</b>", _("Monitor"));
	    use_color = FALSE;
	}

	ctk_label_set_markup (CTK_LABEL (app->current_monitor_label), str);
	g_free (str);

	if (use_color)
	{
	    CdkRGBA black = { 0, 0, 0, 1.0 };

	    ctk_widget_override_background_color (app->current_monitor_event_box, ctk_widget_get_state_flags (app->current_monitor_event_box), &color);

	    /* Make the label explicitly black.  We don't want it to follow the
	     * theme's colors, since the label is always shown against a light
	     * pastel background.  See bgo#556050
	     */
	    ctk_widget_override_color (app->current_monitor_label, ctk_widget_get_state_flags (app->current_monitor_label), &black);
	}

	ctk_event_box_set_visible_window (CTK_EVENT_BOX (app->current_monitor_event_box), use_color);
}

static void
rebuild_on_off_radios (App *app)
{
    gboolean sensitive;
    gboolean on_active;
    gboolean off_active;

    g_signal_handlers_block_by_func (app->monitor_on_radio, G_CALLBACK (monitor_on_off_toggled_cb), app);
    g_signal_handlers_block_by_func (app->monitor_off_radio, G_CALLBACK (monitor_on_off_toggled_cb), app);

    sensitive = FALSE;
    on_active = FALSE;
    off_active = FALSE;

    if (!cafe_rr_config_get_clone (app->current_configuration) && app->current_output)
    {
	if (count_active_outputs (app) > 1 || !cafe_rr_output_info_is_active (app->current_output))
	    sensitive = TRUE;
	else
	    sensitive = FALSE;

	on_active = cafe_rr_output_info_is_active (app->current_output);
	off_active = !on_active;
    }

    ctk_widget_set_sensitive (app->monitor_on_radio, sensitive);
    ctk_widget_set_sensitive (app->monitor_off_radio, sensitive);

    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (app->monitor_on_radio), on_active);
    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (app->monitor_off_radio), off_active);

    g_signal_handlers_unblock_by_func (app->monitor_on_radio, G_CALLBACK (monitor_on_off_toggled_cb), app);
    g_signal_handlers_unblock_by_func (app->monitor_off_radio, G_CALLBACK (monitor_on_off_toggled_cb), app);
}

static char *
make_resolution_string (int width, int height)
{
    return g_strdup_printf (_("%d x %d"), width, height);
}

static void
find_best_mode (CafeRRMode **modes, int *out_width, int *out_height)
{
    int i;

    *out_width = 0;
    *out_height = 0;

    for (i = 0; modes[i] != NULL; i++)
    {
	int w, h;

	w = cafe_rr_mode_get_width (modes[i]);
	h = cafe_rr_mode_get_height (modes[i]);

	if (w * h > *out_width * *out_height)
	{
	    *out_width = w;
	    *out_height = h;
	}
    }
}

static void
rebuild_resolution_combo (App *app)
{
    int i;
    CafeRRMode **modes;
    const char *current;
    int output_width, output_height;

    clear_combo (app->resolution_combo);

    if (!(modes = get_current_modes (app))
	|| !app->current_output
	|| !cafe_rr_output_info_is_active (app->current_output))
    {
	ctk_widget_set_sensitive (app->resolution_combo, FALSE);
	return;
    }

    g_assert (app->current_output != NULL);

    cafe_rr_output_info_get_geometry (app->current_output, NULL, NULL, &output_width, &output_height);
    g_assert (output_width != 0 && output_height != 0);

    ctk_widget_set_sensitive (app->resolution_combo, TRUE);

    for (i = 0; modes[i] != NULL; ++i)
    {
	int width, height;

	width = cafe_rr_mode_get_width (modes[i]);
	height = cafe_rr_mode_get_height (modes[i]);

	add_key (app->resolution_combo,
		 idle_free (make_resolution_string (width, height)),
		 width, height, 0, -1);
    }

    current = idle_free (make_resolution_string (output_width, output_height));

    if (!combo_select (app->resolution_combo, current))
    {
	int best_w, best_h;

	find_best_mode (modes, &best_w, &best_h);
	combo_select (app->resolution_combo, idle_free (make_resolution_string (best_w, best_h)));
    }
}

static void
rebuild_gui (App *app)
{
    gboolean sensitive;

    /* We would break spectacularly if we recursed, so
     * just assert if that happens
     */
    g_assert (app->ignore_gui_changes == FALSE);

    app->ignore_gui_changes = TRUE;

    sensitive = app->current_output ? TRUE : FALSE;

#if 0
    g_debug ("rebuild gui, is on: %d", cafe_rr_output_info_is_active (app->current_output));
#endif

    rebuild_mirror_screens (app);
    rebuild_current_monitor_label (app);
    rebuild_on_off_radios (app);
    rebuild_resolution_combo (app);
    rebuild_rate_combo (app);
    rebuild_rotation_combo (app);

#if 0
    g_debug ("sensitive: %d, on: %d", sensitive, cafe_rr_output_info_is_active (app->current_output));
#endif
    ctk_widget_set_sensitive (app->panel_checkbox, sensitive);

    ctk_widget_set_sensitive (app->primary_button, app->current_output && !cafe_rr_output_info_get_primary(app->current_output));

    app->ignore_gui_changes = FALSE;
}

static gboolean
get_mode (CtkWidget *widget, int *width, int *height, int *freq, CafeRRRotation *rot)
{
    CtkTreeIter iter;
    CtkTreeModel *model;
    CtkComboBox *box = CTK_COMBO_BOX (widget);
    int dummy;

    if (!ctk_combo_box_get_active_iter (box, &iter))
	return FALSE;

    if (!width)
	width = &dummy;

    if (!height)
	height = &dummy;

    if (!freq)
	freq = &dummy;

    if (!rot)
	rot = (CafeRRRotation *)&dummy;

    model = ctk_combo_box_get_model (box);
    ctk_tree_model_get (model, &iter,
			1, width,
			2, height,
			3, freq,
			5, rot,
			-1);

    return TRUE;

}

static void
on_rotation_changed (CtkComboBox *box, gpointer data)
{
    App *app = data;
    CafeRRRotation rotation;

    if (!app->current_output)
	return;

    if (get_mode (app->rotation_combo, NULL, NULL, NULL, &rotation))
	cafe_rr_output_info_set_rotation (app->current_output, rotation);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
on_rate_changed (CtkComboBox *box, gpointer data)
{
    App *app = data;
    int rate;

    if (!app->current_output)
	return;

    if (get_mode (app->refresh_combo, NULL, NULL, &rate, NULL))
	cafe_rr_output_info_set_refresh_rate (app->current_output, rate);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
select_resolution_for_current_output (App *app)
{
    CafeRRMode **modes;
    int width, height;
    int x, y;
    cafe_rr_output_info_get_geometry (app->current_output, &x, &y, NULL, NULL);

    width = cafe_rr_output_info_get_preferred_width (app->current_output);
    height = cafe_rr_output_info_get_preferred_height (app->current_output);

    if (width != 0 && height != 0)
    {
	cafe_rr_output_info_set_geometry (app->current_output, x, y, width, height);
	return;
    }

    modes = get_current_modes (app);
    if (!modes)
	return;

    find_best_mode (modes, &width, &height);

    cafe_rr_output_info_set_geometry (app->current_output, x, y, width, height);
}

static void
monitor_on_off_toggled_cb (CtkToggleButton *toggle, gpointer data)
{
    App *app = data;
    gboolean is_on;

    if (!app->current_output)
	return;

    if (!ctk_toggle_button_get_active (toggle))
	return;

    if (CTK_WIDGET (toggle) == app->monitor_on_radio)
	is_on = TRUE;
    else if (CTK_WIDGET (toggle) == app->monitor_off_radio)
	is_on = FALSE;
    else
    {
	g_assert_not_reached ();
	return;
    }

    cafe_rr_output_info_set_active (app->current_output, is_on);

    if (is_on)
	select_resolution_for_current_output (app); /* The refresh rate will be picked in rebuild_rate_combo() */

    rebuild_gui (app);
    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
realign_outputs_after_resolution_change (App *app, CafeRROutputInfo *output_that_changed, int old_width, int old_height)
{
    /* We find the outputs that were below or to the right of the output that
     * changed, and realign them; we also do that for outputs that shared the
     * right/bottom edges with the output that changed.  The outputs that are
     * above or to the left of that output don't need to change.
     */

    int i;
    int old_right_edge, old_bottom_edge;
    int dx, dy;
    int x, y, width, height;
    CafeRROutputInfo **outputs;

    g_assert (app->current_configuration != NULL);

    cafe_rr_output_info_get_geometry (output_that_changed, &x, &y, &width, &height);
    if (width == old_width && height == old_height)
	return;

    old_right_edge = x + old_width;
    old_bottom_edge = y + old_height;

    dx = width - old_width;
    dy = height - old_height;

    outputs = cafe_rr_config_get_outputs (app->current_configuration);

    for (i = 0; outputs[i] != NULL; i++)
      {
        int output_x, output_y;
	int output_width, output_height;

	if (outputs[i] == output_that_changed || cafe_rr_output_info_is_connected (outputs[i]))
	  continue;

	cafe_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);

	if (output_x >= old_right_edge)
	  output_x += dx;
	else if (output_x + output_width == old_right_edge)
	  output_x = x + width - output_width;


	if (output_y >= old_bottom_edge)
	    output_y += dy;
	else if (output_y + output_height == old_bottom_edge)
	    output_y = y + height - output_height;

	cafe_rr_output_info_set_geometry (outputs[i], output_x, output_y, output_width, output_height);
    }
}

static void
on_resolution_changed (CtkComboBox *box, gpointer data)
{
    App *app = data;
    int old_width, old_height;
    int x, y;
    int width;
    int height;

    if (!app->current_output)
	return;

    cafe_rr_output_info_get_geometry (app->current_output, &x, &y, &old_width, &old_height);

    if (get_mode (app->resolution_combo, &width, &height, NULL, NULL))
    {
	cafe_rr_output_info_set_geometry (app->current_output, x, y, width, height);

	if (width == 0 || height == 0)
	    cafe_rr_output_info_set_active (app->current_output, FALSE);
	else
	    cafe_rr_output_info_set_active (app->current_output, TRUE);
    }

    realign_outputs_after_resolution_change (app, app->current_output, old_width, old_height);

    rebuild_rate_combo (app);
    rebuild_rotation_combo (app);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
lay_out_outputs_horizontally (App *app)
{
    int i;
    int x;
    CafeRROutputInfo **outputs;

    /* Lay out all the monitors horizontally when "mirror screens" is turned
     * off, to avoid having all of them overlapped initially.  We put the
     * outputs turned off on the right-hand side.
     */

    x = 0;

    /* First pass, all "on" outputs */
    outputs = cafe_rr_config_get_outputs (app->current_configuration);

    for (i = 0; outputs[i]; ++i)
    {
	int width, height;
	if (cafe_rr_output_info_is_connected (outputs[i]) &&cafe_rr_output_info_is_active (outputs[i]))
	{
	    cafe_rr_output_info_get_geometry (outputs[i], NULL, NULL, &width, &height);
	    cafe_rr_output_info_set_geometry (outputs[i], x, 0, width, height);
	    x += width;
	}
    }

    /* Second pass, all the black screens */

    for (i = 0; outputs[i]; ++i)
    {
	int width, height;
	if (!(cafe_rr_output_info_is_connected (outputs[i]) && cafe_rr_output_info_is_active (outputs[i])))
	  {
	    cafe_rr_output_info_get_geometry (outputs[i], NULL, NULL, &width, &height);
	    cafe_rr_output_info_set_geometry (outputs[i], x, 0, width, height);
	    x += width;
	}
    }

}

/* FIXME: this function is copied from cafe-settings-daemon/plugins/xrandr/gsd-xrandr-manager.c.
 * Do we need to put this function in cafe-desktop for public use?
 */
static gboolean
get_clone_size (CafeRRScreen *screen, int *width, int *height)
{
        CafeRRMode **modes = cafe_rr_screen_list_clone_modes (screen);
        int best_w, best_h;
        int i;

        best_w = 0;
        best_h = 0;

        for (i = 0; modes[i] != NULL; ++i) {
                CafeRRMode *mode = modes[i];
                int w, h;

                w = cafe_rr_mode_get_width (mode);
                h = cafe_rr_mode_get_height (mode);

                if (w * h > best_w * best_h) {
                        best_w = w;
                        best_h = h;
                }
        }

        if (best_w > 0 && best_h > 0) {
                if (width)
                        *width = best_w;
                if (height)
                        *height = best_h;

                return TRUE;
        }

        return FALSE;
}

static gboolean
output_info_supports_mode (App *app, CafeRROutputInfo *info, int width, int height)
{
    CafeRROutput *output;
    CafeRRMode **modes;
    int i;

    if (!cafe_rr_output_info_is_connected (info))
	return FALSE;

    output = cafe_rr_screen_get_output_by_name (app->screen, cafe_rr_output_info_get_name (info));
    if (!output)
	return FALSE;

    modes = cafe_rr_output_list_modes (output);

    for (i = 0; modes[i]; i++) {
	if (cafe_rr_mode_get_width (modes[i]) == width
	    && cafe_rr_mode_get_height (modes[i]) == height)
	    return TRUE;
    }

    return FALSE;
}

static void
on_clone_changed (CtkWidget *box, gpointer data)
{
    App *app = data;

    cafe_rr_config_set_clone (app->current_configuration, ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (app->clone_checkbox)));

    if (cafe_rr_config_get_clone (app->current_configuration))
    {
	int i;
	int width, height;
	CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (app->current_configuration);

	for (i = 0; outputs[i]; ++i)
	{
	    if (cafe_rr_output_info_is_connected(outputs[i]))
	    {
		app->current_output = outputs[i];
		break;
	    }
	}

	/* Turn on all the connected screens that support the best clone mode.
	 * The user may hit "Mirror Screens", but he shouldn't have to turn on
	 * all the required outputs as well.
	 */

	get_clone_size (app->screen, &width, &height);

	for (i = 0; outputs[i]; i++) {
	    int x, y;
	    if (output_info_supports_mode (app, outputs[i], width, height)) {
		cafe_rr_output_info_set_active (outputs[i], TRUE);
		cafe_rr_output_info_get_geometry (outputs[i], &x, &y, NULL, NULL);
		cafe_rr_output_info_set_geometry (outputs[i], x, y, width, height);
	    }
	}
    }
    else
    {
	if (output_overlaps (app->current_output, app->current_configuration))
	    lay_out_outputs_horizontally (app);
    }

    rebuild_gui (app);
}

static void
get_geometry (CafeRROutputInfo *output, int *w, int *h)
{
    CafeRRRotation rotation;

    if (cafe_rr_output_info_is_active (output))
    {
	cafe_rr_output_info_get_geometry (output, NULL, NULL, w, h);
    }
    else
    {
	*h = cafe_rr_output_info_get_preferred_height (output);
	*w = cafe_rr_output_info_get_preferred_width (output);
    }
   rotation = cafe_rr_output_info_get_rotation (output);
   if ((rotation & CAFE_RR_ROTATION_90) || (rotation & CAFE_RR_ROTATION_270))
   {
        int tmp;
        tmp = *h;
        *h = *w;
        *w = tmp;
   }
}

#define SPACE 15
#define MARGIN  15

static GList *
list_connected_outputs (App *app, int *total_w, int *total_h)
{
    int i, dummy;
    GList *result = NULL;
    CafeRROutputInfo **outputs;

    if (!total_w)
	total_w = &dummy;
    if (!total_h)
	total_h = &dummy;

    *total_w = 0;
    *total_h = 0;

    outputs = cafe_rr_config_get_outputs(app->current_configuration);
    for (i = 0; outputs[i] != NULL; ++i)
    {
	if (cafe_rr_output_info_is_connected (outputs[i]))
	{
	    int w, h;

	    result = g_list_prepend (result, outputs[i]);

	    get_geometry (outputs[i], &w, &h);

	    *total_w += w;
	    *total_h += h;
	}
    }

    return g_list_reverse (result);
}

static int
get_n_connected (App *app)
{
    GList *connected_outputs = list_connected_outputs (app, NULL, NULL);
    int n = g_list_length (connected_outputs);

    g_list_free (connected_outputs);

    return n;
}

static double
compute_scale (App *app)
{
    int available_w, available_h;
    int total_w, total_h;
    int n_monitors;
    CdkRectangle viewport;
    GList *connected_outputs;

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (app->area), &viewport);

    connected_outputs = list_connected_outputs (app, &total_w, &total_h);

    n_monitors = g_list_length (connected_outputs);

    g_list_free (connected_outputs);

    available_w = viewport.width - 2 * MARGIN - (n_monitors - 1) * SPACE;
    available_h = viewport.height - 2 * MARGIN - (n_monitors - 1) * SPACE;

    return MIN ((double)available_w / total_w, (double)available_h / total_h);
}

typedef struct Edge
{
    CafeRROutputInfo *output;
    int x1, y1;
    int x2, y2;
} Edge;

typedef struct Snap
{
    Edge *snapper;		/* Edge that should be snapped */
    Edge *snappee;
    int dy, dx;
} Snap;

static void
add_edge (CafeRROutputInfo *output, int x1, int y1, int x2, int y2, GArray *edges)
{
    Edge e;

    e.x1 = x1;
    e.x2 = x2;
    e.y1 = y1;
    e.y2 = y2;
    e.output = output;

    g_array_append_val (edges, e);
}

static void
list_edges_for_output (CafeRROutputInfo *output, GArray *edges)
{
    int x, y, w, h;

    cafe_rr_output_info_get_geometry (output, &x, &y, &w, &h);
    get_geometry(output, &w, &h); // accounts for rotation

    /* Top, Bottom, Left, Right */
    add_edge (output, x, y, x + w, y, edges);
    add_edge (output, x, y + h, x + w, y + h, edges);
    add_edge (output, x, y, x, y + h, edges);
    add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (CafeRRConfig *config, GArray *edges)
{
    int i;
    CafeRROutputInfo **outputs = cafe_rr_config_get_outputs (config);

    for (i = 0; outputs[i]; ++i)
    {
	if (cafe_rr_output_info_is_connected (outputs[i]))
	    list_edges_for_output (outputs[i], edges);
    }
}

static gboolean
overlap (int s1, int e1, int s2, int e2)
{
    return (!(e1 < s2 || s1 >= e2));
}

static gboolean
horizontal_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->y1 != snapper->y2 || snappee->y1 != snappee->y2)
	return FALSE;

    return overlap (snapper->x1, snapper->x2, snappee->x1, snappee->x2);
}

static gboolean
vertical_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->x1 != snapper->x2 || snappee->x1 != snappee->x2)
	return FALSE;

    return overlap (snapper->y1, snapper->y2, snappee->y1, snappee->y2);
}

static void
add_snap (GArray *snaps, Snap snap)
{
    if (ABS (snap.dx) <= 200 || ABS (snap.dy) <= 200)
	g_array_append_val (snaps, snap);
}

static void
add_edge_snaps (Edge *snapper, Edge *snappee, GArray *snaps)
{
    Snap snap;

    snap.snapper = snapper;
    snap.snappee = snappee;

    if (horizontal_overlap (snapper, snappee))
    {
	snap.dx = 0;
	snap.dy = snappee->y1 - snapper->y1;

	add_snap (snaps, snap);
    }
    else if (vertical_overlap (snapper, snappee))
    {
	snap.dy = 0;
	snap.dx = snappee->x1 - snapper->x1;

	add_snap (snaps, snap);
    }

    /* Corner snaps */
    /* 1->1 */
    snap.dx = snappee->x1 - snapper->x1;
    snap.dy = snappee->y1 - snapper->y1;

    add_snap (snaps, snap);

    /* 1->2 */
    snap.dx = snappee->x2 - snapper->x1;
    snap.dy = snappee->y2 - snapper->y1;

    add_snap (snaps, snap);

    /* 2->2 */
    snap.dx = snappee->x2 - snapper->x2;
    snap.dy = snappee->y2 - snapper->y2;

    add_snap (snaps, snap);

    /* 2->1 */
    snap.dx = snappee->x1 - snapper->x2;
    snap.dy = snappee->y1 - snapper->y2;

    add_snap (snaps, snap);
}

static void
list_snaps (CafeRROutputInfo *output, GArray *edges, GArray *snaps)
{
    int i;

    for (i = 0; i < edges->len; ++i)
    {
	Edge *output_edge = &(g_array_index (edges, Edge, i));

	if (output_edge->output == output)
	{
	    int j;

	    for (j = 0; j < edges->len; ++j)
	    {
		Edge *edge = &(g_array_index (edges, Edge, j));

		if (edge->output != output)
		    add_edge_snaps (output_edge, edge, snaps);
	    }
	}
    }
}

#if 0
static void
print_edge (Edge *edge)
{
    g_debug ("(%d %d %d %d)", edge->x1, edge->y1, edge->x2, edge->y2);
}
#endif

static gboolean
corner_on_edge (int x, int y, Edge *e)
{
    if (x == e->x1 && x == e->x2 && y >= e->y1 && y <= e->y2)
	return TRUE;

    if (y == e->y1 && y == e->y2 && x >= e->x1 && x <= e->x2)
	return TRUE;

    return FALSE;
}

static gboolean
edges_align (Edge *e1, Edge *e2)
{
    if (corner_on_edge (e1->x1, e1->y1, e2))
	return TRUE;

    if (corner_on_edge (e2->x1, e2->y1, e1))
	return TRUE;

    return FALSE;
}

static gboolean
output_is_aligned (CafeRROutputInfo *output, GArray *edges)
{
    gboolean result = FALSE;
    int i;

    for (i = 0; i < edges->len; ++i)
    {
	Edge *output_edge = &(g_array_index (edges, Edge, i));

	if (output_edge->output == output)
	{
	    int j;

	    for (j = 0; j < edges->len; ++j)
	    {
		Edge *edge = &(g_array_index (edges, Edge, j));

		/* We are aligned if an output edge matches
		 * an edge of another output
		 */
		if (edge->output != output_edge->output)
		{
		    if (edges_align (output_edge, edge))
		    {
			result = TRUE;
			goto done;
		    }
		}
	    }
	}
    }
done:

    return result;
}

static void
get_output_rect (CafeRROutputInfo *output, CdkRectangle *rect)
{
    cafe_rr_output_info_get_geometry (output, &rect->x, &rect->y, &rect->width, &rect->height);
    get_geometry (output, &rect->width, &rect->height); // accounts for rotation
}

static gboolean
output_overlaps (CafeRROutputInfo *output, CafeRRConfig *config)
{
    int i;
    CdkRectangle output_rect;
    CafeRROutputInfo **outputs;

    get_output_rect (output, &output_rect);

    outputs = cafe_rr_config_get_outputs (config);
    for (i = 0; outputs[i]; ++i)
    {
	if (outputs[i] != output && cafe_rr_output_info_is_connected (outputs[i]))
	{
	    CdkRectangle other_rect;

	    get_output_rect (outputs[i], &other_rect);
	    if (cdk_rectangle_intersect (&output_rect, &other_rect, NULL))
		return TRUE;
	}
    }

    return FALSE;
}

static gboolean
cafe_rr_config_is_aligned (CafeRRConfig *config, GArray *edges)
{
    int i;
    gboolean result = TRUE;
    CafeRROutputInfo **outputs;

    outputs = cafe_rr_config_get_outputs(config);
    for (i = 0; outputs[i]; ++i)
    {
	if (cafe_rr_output_info_is_connected (outputs[i]))
	{
	    if (!output_is_aligned (outputs[i], edges))
		return FALSE;

	    if (output_overlaps (outputs[i], config))
		return FALSE;
	}
    }

    return result;
}

struct GrabInfo
{
    int grab_x;
    int grab_y;
    int output_x;
    int output_y;
};

static gboolean
is_corner_snap (const Snap *s)
{
    return s->dx != 0 && s->dy != 0;
}

static int
compare_snaps (gconstpointer v1, gconstpointer v2)
{
    const Snap *s1 = v1;
    const Snap *s2 = v2;
    int sv1 = MAX (ABS (s1->dx), ABS (s1->dy));
    int sv2 = MAX (ABS (s2->dx), ABS (s2->dy));
    int d;

    d = sv1 - sv2;

    /* This snapping algorithm is good enough for rock'n'roll, but
     * this is probably a better:
     *
     *    First do a horizontal/vertical snap, then
     *    with the new coordinates from that snap,
     *    do a corner snap.
     *
     * Right now, it's confusing that corner snapping
     * depends on the distance in an axis that you can't actually see.
     *
     */
    if (d == 0)
    {
	if (is_corner_snap (s1) && !is_corner_snap (s2))
	    return -1;
	else if (is_corner_snap (s2) && !is_corner_snap (s1))
	    return 1;
	else
	    return 0;
    }
    else
    {
	return d;
    }
}

/* Sets a mouse cursor for a widget's window.  As a hack, you can pass
 * CDK_BLANK_CURSOR to mean "set the cursor to NULL" (i.e. reset the widget's
 * window's cursor to its default).
 */
static void
set_cursor (CtkWidget *widget, CdkCursorType type)
{
	CdkCursor *cursor;
	CdkWindow *window;

	if (type == CDK_BLANK_CURSOR)
	    cursor = NULL;
	else
	    cursor = cdk_cursor_new_for_display (ctk_widget_get_display (widget), type);

	window = ctk_widget_get_window (widget);

	if (window)
	    cdk_window_set_cursor (window, cursor);

	if (cursor)
	    g_object_unref (cursor);
}

static void
set_monitors_tooltip (App *app, gboolean is_dragging)
{
    const char *text;

    if (is_dragging)
	text = NULL;
    else
	text = _("Select a monitor to change its properties; drag it to rearrange its placement.");

    ctk_widget_set_tooltip_text (app->area, text);
}

static void
on_output_event (FooScrollArea *area,
		 FooScrollAreaEvent *event,
		 gpointer data)
{
    CafeRROutputInfo *output = data;
    App *app = g_object_get_data (G_OBJECT (area), "app");

    /* If the mouse is inside the outputs, set the cursor to "you can move me".  See
     * on_canvas_event() for where we reset the cursor to the default if it
     * exits the outputs' area.
     */
    if (!cafe_rr_config_get_clone (app->current_configuration) && get_n_connected (app) > 1)
	set_cursor (CTK_WIDGET (area), CDK_FLEUR);

    if (event->type == FOO_BUTTON_PRESS)
    {
	GrabInfo *info;

	app->current_output = output;

	rebuild_gui (app);
	set_monitors_tooltip (app, TRUE);

	if (!cafe_rr_config_get_clone (app->current_configuration) && get_n_connected (app) > 1)
	{
	    int output_x, output_y;
	    cafe_rr_output_info_get_geometry (output, &output_x, &output_y, NULL, NULL);

	    foo_scroll_area_begin_grab (area, on_output_event, data);

	    info = g_new0 (GrabInfo, 1);
	    info->grab_x = event->x;
	    info->grab_y = event->y;
	    info->output_x = output_x;
	    info->output_y = output_y;

	    g_object_set_data (G_OBJECT (output), "grab-info", info);
	}

	foo_scroll_area_invalidate (area);
    }
    else
    {
	if (foo_scroll_area_is_grabbed (area))
	{
	    GrabInfo *info = g_object_get_data (G_OBJECT (output), "grab-info");
	    double scale = compute_scale (app);
	    int old_x, old_y;
	    int width, height;
	    int new_x, new_y;
	    int i;
	    GArray *edges, *snaps, *new_edges;

	    cafe_rr_output_info_get_geometry (output, &old_x, &old_y, &width, &height);
	    new_x = info->output_x + (event->x - info->grab_x) / scale;
	    new_y = info->output_y + (event->y - info->grab_y) / scale;

	    cafe_rr_output_info_set_geometry (output, new_x, new_y, width, height);

	    edges = g_array_new (TRUE, TRUE, sizeof (Edge));
	    snaps = g_array_new (TRUE, TRUE, sizeof (Snap));
	    new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

	    list_edges (app->current_configuration, edges);
	    list_snaps (output, edges, snaps);

	    g_array_sort (snaps, compare_snaps);

	    cafe_rr_output_info_set_geometry (output, new_x, new_y, width, height);

	    for (i = 0; i < snaps->len; ++i)
	    {
		Snap *snap = &(g_array_index (snaps, Snap, i));
		GArray *new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

		cafe_rr_output_info_set_geometry (output, new_x + snap->dx, new_y + snap->dy, width, height);

		g_array_set_size (new_edges, 0);
		list_edges (app->current_configuration, new_edges);

		if (cafe_rr_config_is_aligned (app->current_configuration, new_edges))
		{
		    g_array_free (new_edges, TRUE);
		    break;
		}
		else
		{
		    cafe_rr_output_info_set_geometry (output, info->output_x, info->output_y, width, height);
		}
	    }

	    g_array_free (new_edges, TRUE);
	    g_array_free (snaps, TRUE);
	    g_array_free (edges, TRUE);

	    if (event->type == FOO_BUTTON_RELEASE)
	    {
		foo_scroll_area_end_grab (area);
		set_monitors_tooltip (app, FALSE);

		g_free (g_object_get_data (G_OBJECT (output), "grab-info"));
		g_object_set_data (G_OBJECT (output), "grab-info", NULL);

#if 0
		g_debug ("new position: %d %d %d %d", output->x, output->y, output->width, output->height);
#endif
	    }

	    foo_scroll_area_invalidate (area);
	}
    }
}

static void
on_canvas_event (FooScrollArea *area,
		 FooScrollAreaEvent *event,
		 gpointer data)
{
    /* If the mouse exits the outputs, reset the cursor to the default.  See
     * on_output_event() for where we set the cursor to the movement cursor if
     * it is over one of the outputs.
     */
    set_cursor (CTK_WIDGET (area), CDK_BLANK_CURSOR);
}

static PangoLayout *
get_display_name (App *app,
		  CafeRROutputInfo *output)
{
    char *text;
    PangoLayout * layout;

    if (cafe_rr_config_get_clone (app->current_configuration)) {
    /* Translators:  this is the feature where what you see on your laptop's
     * screen is the same as your external monitor.  Here, "Mirror" is being
     * used as an adjective, not as a verb.  For example, the Spanish
     * translation could be "Pantallas en Espejo", *not* "Espejar Pantallas".
     */
        text = g_strdup_printf (_("Mirror Screens"));
    }
    else {
        text = g_strdup_printf ("<b>%s</b>\n<small>%s</small>", cafe_rr_output_info_get_display_name (output), cafe_rr_output_info_get_name (output));
    }
    layout = ctk_widget_create_pango_layout (CTK_WIDGET (app->area), text);
    pango_layout_set_markup (layout, text, -1);
    g_free (text);
    pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
    return layout;
}

static void
paint_background (FooScrollArea *area,
		  cairo_t       *cr)
{
    CdkRectangle viewport;
    CtkWidget *widget;
    CtkStyleContext *widget_style;
    CdkRGBA *base_color = NULL;
    CdkRGBA dark_color;

    widget = CTK_WIDGET (area);

    foo_scroll_area_get_viewport (area, &viewport);

    widget_style = ctk_widget_get_style_context (widget);

    ctk_style_context_save (widget_style);
    ctk_style_context_set_state (widget_style, CTK_STATE_FLAG_SELECTED);
    ctk_style_context_get (widget_style,
                           ctk_style_context_get_state (widget_style),
                           CTK_STYLE_PROPERTY_BACKGROUND_COLOR, &base_color,
                           NULL);
    ctk_style_context_restore (widget_style);
    cdk_cairo_set_source_rgba(cr, base_color);
    cdk_rgba_free (base_color);

    cairo_rectangle (cr,
		     viewport.x, viewport.y,
		     viewport.width, viewport.height);

    cairo_fill_preserve (cr);

    foo_scroll_area_add_input_from_fill (area, cr, on_canvas_event, NULL);

    ctk_style_context_save (widget_style);
    ctk_style_context_set_state (widget_style, CTK_STATE_FLAG_SELECTED);
    cafe_desktop_ctk_style_get_dark_color (widget_style,
                                           ctk_style_context_get_state (widget_style),
                                           &dark_color);
    ctk_style_context_restore (widget_style);
    cdk_cairo_set_source_rgba (cr, &dark_color);

    cairo_stroke (cr);
}

static void
paint_output (App *app, cairo_t *cr, int i)
{
    int w, h;
    double scale = compute_scale (app);
    double x, y;
    int output_x, output_y;
    CafeRRRotation rotation;
    int total_w, total_h;
    GList *connected_outputs = list_connected_outputs (app, &total_w, &total_h);
    CafeRROutputInfo *output = g_list_nth_data (connected_outputs, i);
    PangoLayout *layout = get_display_name (app, output);
    PangoRectangle ink_extent, log_extent;
    CdkRectangle viewport;
    CdkRGBA output_color;
    double r, g, b;
    double available_w;
    double factor;

    cairo_save (cr);

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (app->area), &viewport);

    get_geometry (output, &w, &h);

#if 0
    g_debug ("%s (%p) geometry %d %d %d", output->name, output,
	     w, h, output->rate);
#endif

    viewport.height -= 2 * MARGIN;
    viewport.width -= 2 * MARGIN;

    cafe_rr_output_info_get_geometry (output, &output_x, &output_y, NULL, NULL);
    x = output_x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0;
    y = output_y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0;

#if 0
    g_debug ("scaled: %f %f", x, y);

    g_debug ("scale: %f", scale);

    g_debug ("%f %f %f %f", x, y, w * scale + 0.5, h * scale + 0.5);
#endif

    cairo_translate (cr,
		     x + (w * scale + 0.5) / 2,
		     y + (h * scale + 0.5) / 2);

    /* rotation is already applied in get_geometry */

    rotation = cafe_rr_output_info_get_rotation (output);
    if (rotation & CAFE_RR_REFLECT_X)
	cairo_scale (cr, -1, 1);

    if (rotation & CAFE_RR_REFLECT_Y)
	cairo_scale (cr, 1, -1);

    cairo_translate (cr,
		     - x - (w * scale + 0.5) / 2,
		     - y - (h * scale + 0.5) / 2);


    cairo_rectangle (cr, x, y, w * scale + 0.5, h * scale + 0.5);
    cairo_clip_preserve (cr);

    cafe_rr_labeler_get_rgba_for_output (app->labeler, output, &output_color);
    r = output_color.red;
    g = output_color.green;
    b = output_color.blue;

    if (!cafe_rr_output_info_is_active (output))
    {
	/* If the output is turned off, just darken the selected color */
	r *= 0.2;
	g *= 0.2;
	b *= 0.2;
    }

    cairo_set_source_rgba (cr, r, g, b, 1.0);

    foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (app->area),
					 cr, on_output_event, output);
    cairo_fill (cr);

    if (output == app->current_output)
    {
	cairo_rectangle (cr, x + 2, y + 2, w * scale + 0.5 - 4, h * scale + 0.5 - 4);

	cairo_set_line_width (cr, 4);
	cairo_set_source_rgba (cr, 0.33, 0.43, 0.57, 1.0);
	cairo_stroke (cr);
    }

    cairo_rectangle (cr, x + 0.5, y + 0.5, w * scale + 0.5 - 1, h * scale + 0.5 - 1);

    cairo_set_line_width (cr, 1);
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

    cairo_stroke (cr);
    cairo_set_line_width (cr, 2);

    layout_set_font (layout, "Sans 12");
    pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);

    available_w = w * scale + 0.5 - 6; /* Same as the inner rectangle's width, minus 1 pixel of padding on each side */
    if (available_w < ink_extent.width)
	factor = available_w / ink_extent.width;
    else
	factor = 1.0;

    cairo_move_to (cr,
		   x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
		   y + ((h * scale + 0.5) - factor * log_extent.height) / 2);

    cairo_scale (cr, factor, factor);

    if (cafe_rr_output_info_is_active (output))
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    else
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

    pango_cairo_show_layout (cr, layout);

    cairo_restore (cr);

    g_object_unref (layout);
}

static void
on_area_paint (FooScrollArea *area,
	       cairo_t	     *cr,
	       gpointer	      data)
{
    App *app = data;
    GList *connected_outputs = NULL;
    GList *list;

    paint_background (area, cr);

    if (!app->current_configuration)
	return;

    connected_outputs = list_connected_outputs (app, NULL, NULL);

#if 0
    double scale;
    scale = compute_scale (app);
    g_debug ("scale: %f", scale);
#endif

    for (list = connected_outputs; list != NULL; list = list->next)
    {
	paint_output (app, cr, g_list_position (connected_outputs, list));

	if (cafe_rr_config_get_clone (app->current_configuration))
	    break;
    }
}

static void
make_text_combo (CtkWidget *widget, int sort_column)
{
    CtkComboBox *box = CTK_COMBO_BOX (widget);
    CtkListStore *store = ctk_list_store_new (
	6,
	G_TYPE_STRING,		/* Text */
	G_TYPE_INT,		/* Width */
	G_TYPE_INT,		/* Height */
	G_TYPE_INT,		/* Frequency */
	G_TYPE_INT,		/* Width * Height */
	G_TYPE_INT);		/* Rotation */

    CtkCellRenderer *cell;

    ctk_cell_layout_clear (CTK_CELL_LAYOUT (widget));

    ctk_combo_box_set_model (box, CTK_TREE_MODEL (store));

    cell = ctk_cell_renderer_text_new ();
    ctk_cell_layout_pack_start (CTK_CELL_LAYOUT (box), cell, TRUE);
    ctk_cell_layout_set_attributes (CTK_CELL_LAYOUT (box), cell,
				    "text", 0,
				    NULL);

    if (sort_column != -1)
    {
	ctk_tree_sortable_set_sort_column_id (CTK_TREE_SORTABLE (store),
					      sort_column,
					      CTK_SORT_DESCENDING);
    }
}

static void
compute_virtual_size_for_configuration (CafeRRConfig *config, int *ret_width, int *ret_height)
{
    int i;
    int width, height;
    int output_x, output_y, output_width, output_height;
    CafeRROutputInfo **outputs;

    width = height = 0;

    outputs = cafe_rr_config_get_outputs (config);
    for (i = 0; outputs[i] != NULL; i++)
    {
	if (cafe_rr_output_info_is_active (outputs[i]))
	{
	    cafe_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);
	    width = MAX (width, output_x + output_width);
	    height = MAX (height, output_y + output_height);
	}
    }

    *ret_width = width;
    *ret_height = height;
}

static void
check_required_virtual_size (App *app)
{
    int req_width, req_height;
    int min_width, max_width;
    int min_height, max_height;

    compute_virtual_size_for_configuration (app->current_configuration, &req_width, &req_height);

    cafe_rr_screen_get_ranges (app->screen, &min_width, &max_width, &min_height, &max_height);

#if 0
    g_debug ("X Server supports:");
    g_debug ("min_width = %d, max_width = %d", min_width, max_width);
    g_debug ("min_height = %d, max_height = %d", min_height, max_height);

    g_debug ("Requesting size of %dx%d", req_width, req_height);
#endif

    if (!(min_width <= req_width && req_width <= max_width
	  && min_height <= req_height && req_height <= max_height))
    {
	/* FIXME: present a useful dialog, maybe even before the user tries to Apply */
#if 0
	g_debug ("Your X server needs a larger Virtual size!");
#endif
    }
}

static void
begin_version2_apply_configuration (App *app, CdkWindow *parent_window, guint32 timestamp)
{
    XID parent_window_xid;
    GError *error = NULL;

    parent_window_xid = CDK_WINDOW_XID (parent_window);

    app->proxy = g_dbus_proxy_new_sync (app->connection,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        "org.cafe.SettingsDaemon",
                                        "/org/cafe/SettingsDaemon/XRANDR",
                                        "org.cafe.SettingsDaemon.XRANDR_2",
                                        NULL,
                                        &error);
    if (app->proxy == NULL) {
        g_warning ("Failed to get dbus connection: %s", error->message);
        g_error_free (error);
    } else {
        app->apply_configuration_state = APPLYING_VERSION_2;
        g_dbus_proxy_call (app->proxy,
                           "ApplyConfiguration",
                           g_variant_new ("(xx)", parent_window_xid, timestamp),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) apply_configuration_returned_cb,
                           app);
    }
}

static void
begin_version1_apply_configuration (App *app)
{
    GError *error = NULL;

    app->proxy = g_dbus_proxy_new_sync (app->connection,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        "org.cafe.SettingsDaemon",
                                        "/org/cafe/SettingsDaemon/XRANDR",
                                        "org.cafe.SettingsDaemon.XRANDR",
                                        NULL,
                                        &error);
    if (app->proxy == NULL) {
        g_warning ("Failed to get dbus connection: %s", error->message);
        g_error_free (error);
    } else {
        app->apply_configuration_state = APPLYING_VERSION_1;
        g_dbus_proxy_call (app->proxy,
                           "ApplyConfiguration",
                           g_variant_new ("()"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) apply_configuration_returned_cb,
                           app);
    }
}

static void
ensure_current_configuration_is_saved (void)
{
        CafeRRScreen *rr_screen;
        CafeRRConfig *rr_config;

        /* Normally, cafe_rr_config_save() creates a backup file based on the
         * old monitors.xml.  However, if *that* file didn't exist, there is
         * nothing from which to create a backup.  So, here we'll save the
         * current/unchanged configuration and then let our caller call
         * cafe_rr_config_save() again with the new/changed configuration, so
         * that there *will* be a backup file in the end.
         */

        rr_screen = cafe_rr_screen_new (cdk_screen_get_default (), NULL); /* NULL-GError */
        if (!rr_screen)
                return;

        rr_config = cafe_rr_config_new_current (rr_screen, NULL);
        cafe_rr_config_save (rr_config, NULL); /* NULL-GError */

        g_object_unref (rr_config);
        g_object_unref (rr_screen);
}

/* Callback for g_dbus_proxy_call() */
static void
apply_configuration_returned_cb (GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer data)
{
    GVariant *variant;
    GError *error;
    App *app = data;

    error = NULL;

    if (app->proxy == NULL)
        return;

    variant = g_dbus_proxy_call_finish (app->proxy, res, &error);
    if (variant == NULL) {
        if (app->apply_configuration_state == APPLYING_VERSION_2
                && g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD)) {
            g_error_free (error);

            g_object_unref (app->proxy);
            app->proxy = NULL;

            begin_version1_apply_configuration (app);
            return;
        } else {
            /* We don't pop up an error message; cafe-settings-daemon already does that
             * in case the selected RANDR configuration could not be applied.
             */
            g_error_free (error);
        }
    }

    g_object_unref (app->proxy);
    app->proxy = NULL;

    g_object_unref (app->connection);
    app->connection = NULL;

    ctk_widget_set_sensitive (app->dialog, TRUE);
}

static gboolean
sanitize_and_save_configuration (App *app)
{
    GError *error;

    cafe_rr_config_sanitize (app->current_configuration);

    check_required_virtual_size (app);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));

    ensure_current_configuration_is_saved ();

    error = NULL;
    if (!cafe_rr_config_save (app->current_configuration, &error))
    {
	error_message (app, _("Could not save the monitor configuration"), error->message);
	g_error_free (error);
	return FALSE;
    }

    return TRUE;
}

static void
apply (App *app)
{
    GError *error = NULL;

    if (!sanitize_and_save_configuration (app))
	return;

    g_assert (app->connection == NULL);
    g_assert (app->proxy == NULL);

    app->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (app->connection == NULL) {
        error_message (app, _("Could not get session bus while applying display configuration"), error->message);
        g_error_free (error);
        return;
    }

    ctk_widget_set_sensitive (app->dialog, FALSE);

    begin_version2_apply_configuration (app, ctk_widget_get_window (app->dialog), app->apply_button_clicked_timestamp);
}

static void
on_detect_displays (CtkWidget *widget, gpointer data)
{
    App *app = data;
    GError *error;

    error = NULL;
    if (!cafe_rr_screen_refresh (app->screen, &error)) {
	if (error) {
	    error_message (app, _("Could not detect displays"), error->message);
	    g_error_free (error);
	}
    }
}

static void
set_primary (CtkWidget *widget, gpointer data)
{
    App *app = data;
    int i;
    CafeRROutputInfo **outputs;

    if (!app->current_output)
        return;

    outputs = cafe_rr_config_get_outputs (app->current_configuration);
    for (i=0; outputs[i]!=NULL; i++) {
        cafe_rr_output_info_set_primary (outputs[i], outputs[i] == app->current_output);
    }

    ctk_widget_set_sensitive (app->primary_button, !cafe_rr_output_info_get_primary(app->current_output));
}

#define CSD_XRANDR_SCHEMA                 "org.cafe.SettingsDaemon.plugins.xrandr"
#define SHOW_ICON_KEY                     "show-notification-icon"
#define DEFAULT_CONFIGURATION_FILE_KEY    "default-configuration-file"

static void
on_show_icon_toggled (CtkWidget *widget, gpointer data)
{
    CtkToggleButton *tb = CTK_TOGGLE_BUTTON (widget);
    App *app = data;

    g_settings_set_boolean (app->settings, SHOW_ICON_KEY,
			   ctk_toggle_button_get_active (tb));
}

static CafeRROutputInfo *
get_nearest_output (CafeRRConfig *configuration, int x, int y)
{
    int i;
    int nearest_index;
    int nearest_dist;
    CafeRROutputInfo **outputs;

    nearest_index = -1;
    nearest_dist = G_MAXINT;

    outputs = cafe_rr_config_get_outputs (configuration);
    for (i = 0; outputs[i] != NULL; i++)
    {
	int dist_x, dist_y;
	int output_x, output_y, output_width, output_height;

	if (!(cafe_rr_output_info_is_connected(outputs[i]) && cafe_rr_output_info_is_active (outputs[i])))
	    continue;

	cafe_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);
	if (x < output_x)
	    dist_x = output_x - x;
	else if (x >= output_x + output_width)
	    dist_x = x - (output_x + output_width) + 1;
	else
	    dist_x = 0;

	if (y < output_y)
	    dist_y = output_y - y;
	else if (y >= output_y + output_height)
	    dist_y = y - (output_y + output_height) + 1;
	else
	    dist_y = 0;

	if (MIN (dist_x, dist_y) < nearest_dist)
	{
	    nearest_dist = MIN (dist_x, dist_y);
	    nearest_index = i;
	}
    }

    if (nearest_index != -1)
	return outputs[nearest_index];
    else
	return NULL;

}

/* Gets the output that contains the largest intersection with the window.
 * Logic stolen from cdk_screen_get_monitor_at_window().
 */
static CafeRROutputInfo *
get_output_for_window (CafeRRConfig *configuration, CdkWindow *window)
{
    CdkRectangle win_rect;
    int i;
    int largest_area;
    int largest_index;
    CafeRROutputInfo **outputs;

    cdk_window_get_geometry (window, &win_rect.x, &win_rect.y, &win_rect.width, &win_rect.height);
    cdk_window_get_origin (window, &win_rect.x, &win_rect.y);

    largest_area = 0;
    largest_index = -1;

    outputs = cafe_rr_config_get_outputs (configuration);
    for (i = 0; outputs[i] != NULL; i++)
    {
	CdkRectangle output_rect, intersection;

	cafe_rr_output_info_get_geometry (outputs[i], &output_rect.x, &output_rect.y, &output_rect.width, &output_rect.height);

	if (cafe_rr_output_info_is_connected (outputs[i]) && cdk_rectangle_intersect (&win_rect, &output_rect, &intersection))
	{
	    int area;

	    area = intersection.width * intersection.height;
	    if (area > largest_area)
	    {
		largest_area = area;
		largest_index = i;
	    }
	}
    }

    if (largest_index != -1)
	return outputs[largest_index];
    else
	return get_nearest_output (configuration,
				   win_rect.x + win_rect.width / 2,
				   win_rect.y + win_rect.height / 2);
}

/* We select the current output, i.e. select the one being edited, based on
 * which output is showing the configuration dialog.
 */
static void
select_current_output_from_dialog_position (App *app)
{
    if (ctk_widget_get_realized (app->dialog))
	app->current_output = get_output_for_window (app->current_configuration, ctk_widget_get_window (app->dialog));
    else
	app->current_output = NULL;

    rebuild_gui (app);
}

/* This is a CtkWidget::map-event handler.  We wait for the display-properties
 * dialog to be mapped, and then we select the output which corresponds to the
 * monitor on which the dialog is being shown.
 */
static gboolean
dialog_map_event_cb (CtkWidget *widget, CdkEventAny *event, gpointer data)
{
    App *app = data;

    select_current_output_from_dialog_position (app);
    return FALSE;
}

static void
apply_button_clicked_cb (CtkButton *button, gpointer data)
{
    App *app = data;

    /* We simply store the timestamp at which the Apply button was clicked.
     * We'll just wait for the dialog to return from ctk_dialog_run(), and
     * *then* use the timestamp when applying the RANDR configuration.
     */

    app->apply_button_clicked_timestamp = ctk_get_current_event_time ();
}

static void
success_dialog_for_make_default (App *app)
{
    CtkWidget *dialog;

    dialog = ctk_message_dialog_new (CTK_WINDOW (app->dialog),
				     CTK_DIALOG_MODAL,
				     CTK_MESSAGE_INFO,
				     CTK_BUTTONS_OK,
				     _("The monitor configuration has been saved"));
    ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
					      _("This configuration will be used the next time someone logs in."));

    ctk_dialog_run (CTK_DIALOG (dialog));
    ctk_widget_destroy (dialog);
}

static void
error_dialog_for_make_default (App *app, const char *error_text)
{
    error_message (app, _("Could not set the default configuration for monitors"), error_text);
}

static void
make_default (App *app)
{
    char *command_line;
    char *source_filename;
    char *dest_filename;
    char *dest_basename;
    char *std_error;
    gint exit_status;
    GError *error;

    if (!sanitize_and_save_configuration (app))
	return;

    dest_filename = g_settings_get_string (app->settings, DEFAULT_CONFIGURATION_FILE_KEY);
    if (!dest_filename)
	return; /* FIXME: present an error? */

    dest_basename = g_path_get_basename (dest_filename);

    source_filename = cafe_rr_config_get_intended_filename ();

    command_line = g_strdup_printf ("pkexec %s/cafe-display-properties-install-systemwide %s %s",
				    SBINDIR,
				    source_filename,
				    dest_basename);

    error = NULL;
    if (!g_spawn_command_line_sync (command_line, NULL, &std_error, &exit_status, &error))
    {
	error_dialog_for_make_default (app, error->message);
	g_error_free (error);
    }
    else if (!WIFEXITED (exit_status) || WEXITSTATUS (exit_status) != 0)
    {
	error_dialog_for_make_default (app, std_error);
    }
    else
    {
	success_dialog_for_make_default (app);
    }

    g_free (std_error);
    g_free (dest_filename);
    g_free (dest_basename);
    g_free (source_filename);
    g_free (command_line);
}

static CtkWidget*
_ctk_builder_get_widget (CtkBuilder *builder, const gchar *name)
{
    return CTK_WIDGET (ctk_builder_get_object (builder, name));
}

static void
run_application (App *app)
{
    CtkBuilder *builder;
    CtkWidget *align;
    GError *error = NULL;

    builder = ctk_builder_new ();

    if (ctk_builder_add_from_resource (builder, "/org/cafe/mcc/display/display-capplet.ui", &error) == 0)
    {
	g_warning ("Could not parse UI definition: %s", error->message);
	g_error_free (error);
	g_object_unref (builder);
	return;
    }

    app->screen = cafe_rr_screen_new (cdk_screen_get_default (), &error);
    g_signal_connect (app->screen, "changed", G_CALLBACK (on_screen_changed), app);
    if (!app->screen)
    {
	error_message (NULL, _("Could not get screen information"), error->message);
	g_error_free (error);
	g_object_unref (builder);
	return;
    }

    app->settings = g_settings_new (CSD_XRANDR_SCHEMA);

    app->dialog = _ctk_builder_get_widget (builder, "dialog");
    g_signal_connect_after (app->dialog, "map-event",
			    G_CALLBACK (dialog_map_event_cb), app);

    ctk_window_set_default_icon_name ("preferences-desktop-display");
    ctk_window_set_icon_name (CTK_WINDOW (app->dialog),
			      "preferences-desktop-display");

    app->current_monitor_event_box = _ctk_builder_get_widget (builder,
    						   "current_monitor_event_box");
    app->current_monitor_label = _ctk_builder_get_widget (builder,
    						       "current_monitor_label");

    app->monitor_on_radio = _ctk_builder_get_widget (builder,
    						     "monitor_on_radio");
    app->monitor_off_radio = _ctk_builder_get_widget (builder,
    						      "monitor_off_radio");
    g_signal_connect (app->monitor_on_radio, "toggled",
		      G_CALLBACK (monitor_on_off_toggled_cb), app);
    g_signal_connect (app->monitor_off_radio, "toggled",
		      G_CALLBACK (monitor_on_off_toggled_cb), app);

    app->resolution_combo = _ctk_builder_get_widget (builder,
    						     "resolution_combo");
    g_signal_connect (app->resolution_combo, "changed",
		      G_CALLBACK (on_resolution_changed), app);

    app->refresh_combo = _ctk_builder_get_widget (builder, "refresh_combo");
    g_signal_connect (app->refresh_combo, "changed",
		      G_CALLBACK (on_rate_changed), app);

    app->rotation_combo = _ctk_builder_get_widget (builder, "rotation_combo");
    g_signal_connect (app->rotation_combo, "changed",
		      G_CALLBACK (on_rotation_changed), app);

    app->clone_checkbox = _ctk_builder_get_widget (builder, "clone_checkbox");
    g_signal_connect (app->clone_checkbox, "toggled",
		      G_CALLBACK (on_clone_changed), app);

    g_signal_connect (_ctk_builder_get_widget (builder, "detect_displays_button"),
		      "clicked", G_CALLBACK (on_detect_displays), app);

    app->primary_button = _ctk_builder_get_widget (builder, "primary_button");

    g_signal_connect (app->primary_button, "clicked", G_CALLBACK (set_primary), app);

    app->show_icon_checkbox = _ctk_builder_get_widget (builder,
						      "show_notification_icon");

    ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (app->show_icon_checkbox),
				  g_settings_get_boolean (app->settings, SHOW_ICON_KEY));

    g_signal_connect (app->show_icon_checkbox, "toggled", G_CALLBACK (on_show_icon_toggled), app);

    app->panel_checkbox = _ctk_builder_get_widget (builder, "panel_checkbox");

    make_text_combo (app->resolution_combo, 4);
    make_text_combo (app->refresh_combo, 3);
    make_text_combo (app->rotation_combo, -1);

    g_assert (app->panel_checkbox);

    /* Scroll Area */
    app->area = (CtkWidget *)foo_scroll_area_new ();

    g_object_set_data (G_OBJECT (app->area), "app", app);

    set_monitors_tooltip (app, FALSE);

    /* FIXME: this should be computed dynamically */
    foo_scroll_area_set_min_size (FOO_SCROLL_AREA (app->area), -1, 200);
    ctk_widget_show (app->area);
    g_signal_connect (app->area, "paint",
		      G_CALLBACK (on_area_paint), app);
    g_signal_connect (app->area, "viewport_changed",
		      G_CALLBACK (on_viewport_changed), app);

    align = _ctk_builder_get_widget (builder, "align");

    ctk_container_add (CTK_CONTAINER (align), app->area);

    app->apply_button = _ctk_builder_get_widget (builder, "apply_button");
    g_signal_connect (app->apply_button, "clicked",
		      G_CALLBACK (apply_button_clicked_cb), app);

    on_screen_changed (app->screen, app);

    g_object_unref (builder);

restart:
    switch (ctk_dialog_run (CTK_DIALOG (app->dialog)))
    {
    default:
	/* Fall Through */
    case CTK_RESPONSE_DELETE_EVENT:
    case CTK_RESPONSE_CLOSE:
#if 0
	g_debug ("Close");
#endif
	break;

    case CTK_RESPONSE_HELP:
        ctk_show_uri_on_window (CTK_DIALOG (app->dialog),
                                "help:cafe-user-guide/goscustdesk-70",
                                ctk_get_current_event_time (),
                                &error);
        if (error)
        {
            error_message (app, _("Could not open help content"), error->message);
            g_error_free (error);
        }
	goto restart;
	break;

    case CTK_RESPONSE_APPLY:
	apply (app);
	goto restart;
	break;

    case RESPONSE_MAKE_DEFAULT:
	make_default (app);
	goto restart;
	break;
    }

    ctk_widget_destroy (app->dialog);
    g_object_unref (app->screen);
    g_object_unref (app->settings);
}

int
main (int argc, char **argv)
{
    App *app;

    capplet_init (NULL, &argc, &argv);

    app = g_new0 (App, 1);

    run_application (app);

    g_free (app);

    return 0;
}
