/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Jonathan Blandford <jrb@gnome.org>
 *            Jens Granseuer <jensgr@gmx.net>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "appearance.h"

#include <string.h>
#include <stdarg.h>
#include <math.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <cdk/cdkx.h>
#include <glib/gstdio.h>

#include "capplet-util.h"

/* X servers sometimes lie about the screen's physical dimensions, so we cannot
 * compute an accurate DPI value.  When this happens, the user gets fonts that
 * are too huge or too tiny.  So, we see what the server returns:  if it reports
 * something outside of the range [DPI_LOW_REASONABLE_VALUE,
 * DPI_HIGH_REASONABLE_VALUE], then we assume that it is lying and we use
 * DPI_FALLBACK instead.
 *
 * See get_dpi_from_cafe_conf_or_server() below, and also
 * https://bugzilla.novell.com/show_bug.cgi?id=217790
 */
#define DPI_FALLBACK 96
#define DPI_LOW_REASONABLE_VALUE 50
#define DPI_HIGH_REASONABLE_VALUE 500

static gboolean in_change = FALSE;

static void sample_draw(CtkWidget* darea, cairo_t* cr)
{
	cairo_surface_t* surface = g_object_get_data(G_OBJECT(darea), "sample-surface");
	CtkAllocation allocation;
	int x, y, w, h;

	ctk_widget_get_allocation (darea, &allocation);
	x = allocation.width;
	y = allocation.height;
	w = cairo_image_surface_get_width (surface);
	h = cairo_image_surface_get_height (surface);

	cairo_set_line_width (cr, 1);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_rectangle (cr, 0, 0, x, y);
	cairo_fill_preserve (cr);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_stroke (cr);

	cairo_set_source_surface (cr, surface, (x - w) / 2, (y - h) / 2);

	cairo_paint(cr);
}

typedef enum {
	ANTIALIAS_NONE,
	ANTIALIAS_GRAYSCALE,
	ANTIALIAS_RGBA
} Antialiasing;

typedef enum {
	HINT_NONE,
	HINT_SLIGHT,
	HINT_MEDIUM,
	HINT_FULL
} Hinting;

typedef enum {
	RGBA_RGB,
	RGBA_BGR,
	RGBA_VRGB,
	RGBA_VBGR
} RgbaOrder;

static void set_fontoptions(PangoContext *context, Antialiasing antialiasing, Hinting hinting)
{
	cairo_font_options_t *opt;
	cairo_antialias_t aa;
	cairo_hint_style_t hs;

	switch (antialiasing) {
	case ANTIALIAS_NONE:
		aa = CAIRO_ANTIALIAS_NONE;
		break;
	case ANTIALIAS_GRAYSCALE:
		aa = CAIRO_ANTIALIAS_GRAY;
		break;
	case ANTIALIAS_RGBA:
		aa = CAIRO_ANTIALIAS_SUBPIXEL;
		break;
	default:
		aa = CAIRO_ANTIALIAS_DEFAULT;
		break;
	}

	switch (hinting) {
	case HINT_NONE:
		hs = CAIRO_HINT_STYLE_NONE;
		break;
	case HINT_SLIGHT:
		hs = CAIRO_HINT_STYLE_SLIGHT;
		break;
	case HINT_MEDIUM:
		hs = CAIRO_HINT_STYLE_MEDIUM;
		break;
	case HINT_FULL:
		hs = CAIRO_HINT_STYLE_FULL;
		break;
	default:
		hs = CAIRO_HINT_STYLE_DEFAULT;
		break;
	}

	opt = cairo_font_options_create ();
	cairo_font_options_set_antialias (opt, aa);
	cairo_font_options_set_hint_style (opt, hs);
	pango_cairo_context_set_font_options (context, opt);
	cairo_font_options_destroy (opt);
}

static void setup_font_sample(CtkWidget* darea, Antialiasing antialiasing, Hinting hinting)
{
	const char *str = "<span font=\"18\" style=\"normal\">abcfgop AO </span>"
					  "<span font=\"20\" style=\"italic\">abcfgop</span>";

	PangoContext *context;
	PangoLayout *layout;
	PangoFontDescription *fd;
	PangoRectangle extents;
	cairo_surface_t *surface;
	cairo_t *cr;
	int width, height;

	context = ctk_widget_get_pango_context (darea);
	set_fontoptions (context, antialiasing, hinting);
	layout = pango_layout_new (context);

	fd = pango_font_description_from_string ("Serif");
	pango_layout_set_font_description (layout, fd);
	pango_font_description_free (fd);

	pango_layout_set_markup (layout, str, -1);

	pango_layout_get_extents (layout, NULL, &extents);
	width = PANGO_PIXELS(extents.width) + 4;
	height = PANGO_PIXELS(extents.height) + 2;

	surface = cairo_image_surface_create (CAIRO_FORMAT_A8, width, height);
	cr = cairo_create (surface);

	cairo_move_to (cr, 2, 1);
	pango_cairo_show_layout (cr, layout);
	g_object_unref (layout);
	cairo_destroy (cr);

	g_object_set_data_full(G_OBJECT(darea), "sample-surface", surface, (GDestroyNotify) cairo_surface_destroy);

	ctk_widget_set_size_request (CTK_WIDGET(darea), width + 2, height + 2);
	g_signal_connect(darea, "draw", G_CALLBACK(sample_draw), NULL);
}

/*
 * Code implementing a group of radio buttons with different cairo option combinations.
 * If one of the buttons is matched by the GSettings key, we pick it. Otherwise we
 * show the group as inconsistent.
 */

typedef struct {
  Antialiasing antialiasing;
  Hinting hinting;
  CtkToggleButton *radio;
} FontPair;

static GSList *font_pairs = NULL;

static void
font_render_load (GSettings *settings)
{
  Antialiasing antialiasing;
  Hinting hinting;
  gboolean inconsistent = TRUE;
  GSList *tmp_list;

  antialiasing = g_settings_get_enum (settings, FONT_ANTIALIASING_KEY);
  hinting = g_settings_get_enum (settings, FONT_HINTING_KEY);

  in_change = TRUE;

  for (tmp_list = font_pairs; tmp_list; tmp_list = tmp_list->next) {
    FontPair *pair = tmp_list->data;

    if (antialiasing == pair->antialiasing && hinting == pair->hinting) {
      ctk_toggle_button_set_active (pair->radio, TRUE);
      inconsistent = FALSE;
      break;
    }
  }

  for (tmp_list = font_pairs; tmp_list; tmp_list = tmp_list->next) {
    FontPair *pair = tmp_list->data;

    ctk_toggle_button_set_inconsistent (pair->radio, inconsistent);
  }

  in_change = FALSE;
}

static void
font_render_changed (GSettings *settings,
                     gchar     *key,
                     gpointer   user_data)
{
  font_render_load (settings);
}

static void
font_radio_toggled (CtkToggleButton *toggle_button,
		    FontPair        *pair)
{
  if (!in_change) {
    GSettings *settings = g_settings_new (FONT_RENDER_SCHEMA);

    g_settings_set_enum (settings, FONT_ANTIALIASING_KEY, pair->antialiasing);
    g_settings_set_enum (settings, FONT_HINTING_KEY, pair->hinting);

    /* Restore back to the previous state until we get notification */
    font_render_load (settings);
    g_object_unref (settings);
  }
}

static void
setup_font_pair (CtkWidget    *radio,
		 CtkWidget    *darea,
		 Antialiasing  antialiasing,
		 Hinting       hinting)
{
  FontPair *pair = g_new (FontPair, 1);

  pair->antialiasing = antialiasing;
  pair->hinting = hinting;
  pair->radio = CTK_TOGGLE_BUTTON (radio);

  setup_font_sample (darea, antialiasing, hinting);
  font_pairs = g_slist_prepend (font_pairs, pair);

  g_signal_connect (radio, "toggled",
		    G_CALLBACK (font_radio_toggled), pair);
}

static void
croma_titlebar_load_sensitivity (AppearanceData *data)
{
  ctk_widget_set_sensitive (appearance_capplet_get_widget (data, "window_title_font"),
			    !g_settings_get_boolean (data->croma_settings,
						    WINDOW_TITLE_USES_SYSTEM_KEY));
}

static void
croma_changed (GSettings *settings,
	       gchar     *entry,
	       gpointer   user_data)
{
  croma_titlebar_load_sensitivity (user_data);
}

/*
 * EnumGroup - a group of radio buttons for a gsettings enum
 */
typedef struct
{
  GSettings *settings;
  GSList *items;
  gchar *settings_key;
  gulong settings_signal_id;
} EnumGroup;

typedef struct
{
  EnumGroup *group;
  CtkToggleButton *widget;
  int value;
} EnumItem;

static void
enum_group_load (EnumGroup *group)
{
  gint val = g_settings_get_enum (group->settings, group->settings_key);
  GSList *tmp_list;

  in_change = TRUE;

  for (tmp_list = group->items; tmp_list; tmp_list = tmp_list->next) {
    EnumItem *item = tmp_list->data;

    if (val == item->value)
      ctk_toggle_button_set_active (item->widget, TRUE);
  }

  in_change = FALSE;
}

static void
enum_group_changed (GSettings *settings,
		    gchar     *key,
		    gpointer  user_data)
{
  enum_group_load (user_data);
}

static void
enum_item_toggled (CtkToggleButton *toggle_button,
		   EnumItem        *item)
{
  EnumGroup *group = item->group;

  if (!in_change) {
    g_settings_set_enum (group->settings, group->settings_key, item->value);
  }

  /* Restore back to the previous state until we get notification */
  enum_group_load (group);
}

static EnumGroup *
enum_group_create (GSettings           *settings,
		   const gchar         *settings_key,
		   CtkWidget           *first_widget,
		   ...)
{
  EnumGroup *group;
  CtkWidget *widget;
  va_list args;

  group = g_new (EnumGroup, 1);

  group->settings = g_object_ref (settings);
  group->settings_key = g_strdup (settings_key);
  group->items = NULL;

  va_start (args, first_widget);

  widget = first_widget;
  while (widget) {
    EnumItem *item;

    item = g_new (EnumItem, 1);
    item->group = group;
    item->widget = CTK_TOGGLE_BUTTON (widget);
    item->value = va_arg (args, int);

    g_signal_connect (item->widget, "toggled",
		      G_CALLBACK (enum_item_toggled), item);

    group->items = g_slist_prepend (group->items, item);

    widget = va_arg (args, CtkWidget *);
  }

  va_end (args);

  enum_group_load (group);

  gchar *signal_name = g_strdup_printf("changed::%s", settings_key);
  group->settings_signal_id = g_signal_connect (settings, signal_name,
                                                G_CALLBACK (enum_group_changed), group);
  g_free (signal_name);

  return group;
}

static void
enum_group_destroy (EnumGroup *group)
{
  g_signal_handler_disconnect (group->settings, group->settings_signal_id);
  g_clear_object (&group->settings);
  group->settings_signal_id = 0;
  g_free (group->settings_key);

  g_slist_foreach (group->items, (GFunc) g_free, NULL);
  g_slist_free (group->items);

  g_free (group);
}

static double
dpi_from_pixels_and_mm (int pixels, int mm)
{
  double dpi;

  if (mm >= 1)
    dpi = pixels / (mm / 25.4);
  else
    dpi = 0;

  return dpi;
}

static double
get_dpi_from_x_server (void)
{
  CdkScreen  *screen;
  double dpi;

  screen = cdk_screen_get_default ();

  if (screen) {
    double width_dpi, height_dpi;

    Screen *xscreen = cdk_x11_screen_get_xscreen (screen);

    width_dpi = dpi_from_pixels_and_mm (WidthOfScreen (xscreen), WidthMMOfScreen (xscreen));
    height_dpi = dpi_from_pixels_and_mm (HeightOfScreen (xscreen), HeightMMOfScreen (xscreen));

    if (width_dpi < DPI_LOW_REASONABLE_VALUE || width_dpi > DPI_HIGH_REASONABLE_VALUE ||
        height_dpi < DPI_LOW_REASONABLE_VALUE || height_dpi > DPI_HIGH_REASONABLE_VALUE)
      dpi = DPI_FALLBACK;
    else
      dpi = (width_dpi + height_dpi) / 2.0;
  } else {
    /* Huh!?  No screen? */
    dpi = DPI_FALLBACK;
  }

  return dpi;
}

/*
 * The font rendering details dialog
 */
static void
dpi_load (GSettings     *settings,
	  CtkSpinButton *spinner)
{
  CdkScreen *screen;
  gint scale;
  gdouble dpi;

  screen = cdk_screen_get_default ();
  scale = cdk_window_get_scale_factor (cdk_screen_get_root_window (screen));
  dpi = g_settings_get_double (settings, FONT_DPI_KEY);

  if (dpi == 0)
    dpi = get_dpi_from_x_server ();

  dpi *= (double)scale;
  dpi = CLAMP(dpi, DPI_LOW_REASONABLE_VALUE, DPI_HIGH_REASONABLE_VALUE);

  in_change = TRUE;
  ctk_spin_button_set_value (spinner, dpi);
  in_change = FALSE;
}

static void
dpi_changed (GSettings      *settings,
	     gchar          *key,
	     AppearanceData *data)
{
  CtkWidget *spinner;
  CtkWidget *toggle;
  gdouble dpi;

  dpi = g_settings_get_double (data->font_settings, FONT_DPI_KEY);
  spinner = appearance_capplet_get_widget (data, "dpi_spinner");
  toggle = appearance_capplet_get_widget (data, "dpi_reset_switch");

  dpi_load (settings, CTK_SPIN_BUTTON (spinner));

  ctk_switch_set_state (CTK_SWITCH (toggle), dpi == 0);
  ctk_widget_set_sensitive (spinner, dpi != 0);
}

static void
monitors_changed (CdkScreen      *screen,
		  AppearanceData *data)
{
  CtkWidget *widget;
  widget = appearance_capplet_get_widget (data, "dpi_spinner");
  dpi_load (data->font_settings, CTK_SPIN_BUTTON (widget));
}

static void
dpi_value_changed (CtkSpinButton  *spinner,
		   AppearanceData *data)
{
  /* Like any time when using a spin button with GSettings, there is
   * a race condition here. When we change, we send the new
   * value to GSettings, then restore to the old value until
   * we get a response to emulate the proper model/view behavior.
   *
   * If the user changes the value faster than responses are
   * received from GSettings, this may cause mildly strange effects.
   */
  if (!in_change) {
    CdkScreen *screen;
    CtkWidget *toggle;
    gint scale;
    gdouble new_dpi;

    screen = cdk_screen_get_default ();
    scale = cdk_window_get_scale_factor (cdk_screen_get_root_window (screen));
    new_dpi = ctk_spin_button_get_value (spinner) / (double)scale;

    g_settings_set_double (data->font_settings, FONT_DPI_KEY, new_dpi);

    dpi_load (data->font_settings, spinner);

    toggle = appearance_capplet_get_widget (data, "dpi_reset_switch");
    ctk_switch_set_active (CTK_SWITCH (toggle), FALSE);
  }
}

static gboolean
dpi_value_reset (CtkSwitch      *toggle,
		 gboolean        state,
		 AppearanceData *data)
{
  CtkWidget *spinner;
  spinner = appearance_capplet_get_widget (data, "dpi_spinner");

  if (state)
    g_settings_set_double (data->font_settings, FONT_DPI_KEY, 0);
  else
    dpi_value_changed (CTK_SPIN_BUTTON (spinner), data);

  ctk_switch_set_state(toggle, state);
  ctk_widget_set_sensitive (spinner, !state);

  return TRUE;
}

static void
cb_details_response (CtkDialog *dialog, gint response_id)
{
  if (response_id == CTK_RESPONSE_HELP) {
    capplet_help (CTK_WINDOW (dialog),
		  "goscustdesk-38");
  } else
    ctk_widget_hide (CTK_WIDGET (dialog));
}

static void install_new_font (const gchar *filepath)
{
    GFile *src, *dst;
    gchar *fontdir, *fontpath;
    char *basename;
    GError *error = NULL;

    fontdir = g_build_filename (g_get_home_dir(), ".fonts", NULL);
    if (!g_file_test (fontdir, G_FILE_TEST_IS_DIR))
    {
        if(g_mkdir (fontdir, 0755) != 0) {
            g_free (fontdir);
            return;
        }
    }
    g_free (fontdir);

    basename = g_path_get_basename (filepath);
    fontpath = g_build_filename (g_get_home_dir(), ".fonts", basename, NULL);
    g_free (basename);
    src = g_file_new_for_path (filepath);
    dst = g_file_new_for_path (fontpath);
    g_free (fontpath);

    if (!g_file_copy (src,
                      dst,
                      G_FILE_COPY_NONE,
                      NULL,
                      NULL,
                      NULL,
                      &error)) {
        g_warning ("install new font failed: %s\n", error->message);
        g_error_free (error);
    }
    g_object_unref (src);
    g_object_unref (dst);
}

static void
cb_add_new_font (CtkWidget *button,
                 AppearanceData *data)
{
    CtkWidget *dialog;
    CtkFileFilter *filter;
    CtkFileChooser *chooser;
    CtkFileChooserAction action = CTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = ctk_file_chooser_dialog_new (_("Select Font"),
            CTK_WINDOW (appearance_capplet_get_widget (data, "appearance_window")),
            action,
            _("_Cancel"),
            CTK_RESPONSE_CANCEL,
            _("_Open"),
            CTK_RESPONSE_ACCEPT,
            NULL);
    chooser = CTK_FILE_CHOOSER (dialog);
    filter = ctk_file_filter_new ();
    ctk_file_filter_set_name (filter, _("Fonts"));
    ctk_file_filter_add_mime_type (filter, "font/ttf");
    ctk_file_chooser_add_filter (chooser, filter);

    res = ctk_dialog_run (CTK_DIALOG (dialog));
    if (res == CTK_RESPONSE_ACCEPT)
    {
        char *filename;
        filename = ctk_file_chooser_get_filename (chooser);
        install_new_font (filename);
        g_free (filename);
    }
    ctk_widget_destroy (dialog);
}

static void
cb_show_details (CtkWidget *button,
		 AppearanceData *data)
{
  if (!data->font_details) {
    CtkAdjustment *adjustment;
    CtkWidget *spinner;
    CtkWidget *toggle;
    EnumGroup *group;
    gdouble dpi;

    data->font_details = appearance_capplet_get_widget (data, "render_details");

    ctk_window_set_transient_for (CTK_WINDOW (data->font_details),
                                  CTK_WINDOW (appearance_capplet_get_widget (data, "appearance_window")));

    spinner = appearance_capplet_get_widget (data, "dpi_spinner");
    toggle = appearance_capplet_get_widget (data, "dpi_reset_switch");

    /* Set initial state for widgets */
    dpi = g_settings_get_double (data->font_settings, FONT_DPI_KEY);
    ctk_switch_set_active (CTK_SWITCH (toggle), dpi == 0);
    ctk_widget_set_sensitive (CTK_WIDGET (spinner), dpi != 0);

    /* pick a sensible maximum dpi */
    adjustment = ctk_spin_button_get_adjustment (CTK_SPIN_BUTTON (spinner));
    ctk_adjustment_set_lower (adjustment, DPI_LOW_REASONABLE_VALUE);
    ctk_adjustment_set_upper (adjustment, DPI_HIGH_REASONABLE_VALUE);
    ctk_adjustment_set_step_increment (adjustment, 1);

    dpi_load (data->font_settings, CTK_SPIN_BUTTON (spinner));
    g_signal_connect (spinner, "value-changed",
		      G_CALLBACK (dpi_value_changed), data);
    g_signal_connect (toggle, "state-set",
		      G_CALLBACK (dpi_value_reset), data);

    g_signal_connect (data->font_settings, "changed::" FONT_DPI_KEY, G_CALLBACK (dpi_changed), data);

    /* Update font DPI when window scaling factor is changed */
    g_signal_connect (cdk_screen_get_default (), "monitors-changed", G_CALLBACK (monitors_changed), data);

    setup_font_sample (appearance_capplet_get_widget (data, "antialias_none_sample"),      ANTIALIAS_NONE,      HINT_SLIGHT);
    setup_font_sample (appearance_capplet_get_widget (data, "antialias_grayscale_sample"), ANTIALIAS_GRAYSCALE, HINT_SLIGHT);
    setup_font_sample (appearance_capplet_get_widget (data, "antialias_subpixel_sample"),  ANTIALIAS_RGBA,      HINT_SLIGHT);

    group = enum_group_create (
    	data->font_settings, FONT_ANTIALIASING_KEY,
	appearance_capplet_get_widget (data, "antialias_none_radio"),      ANTIALIAS_NONE,
	appearance_capplet_get_widget (data, "antialias_grayscale_radio"), ANTIALIAS_GRAYSCALE,
	appearance_capplet_get_widget (data, "antialias_subpixel_radio"),  ANTIALIAS_RGBA,
	NULL);
    data->font_groups = g_slist_prepend (data->font_groups, group);

    setup_font_sample (appearance_capplet_get_widget (data, "hint_none_sample"),   ANTIALIAS_RGBA, HINT_NONE);
    setup_font_sample (appearance_capplet_get_widget (data, "hint_slight_sample"), ANTIALIAS_RGBA, HINT_SLIGHT);
    setup_font_sample (appearance_capplet_get_widget (data, "hint_medium_sample"), ANTIALIAS_RGBA, HINT_MEDIUM);
    setup_font_sample (appearance_capplet_get_widget (data, "hint_full_sample"),   ANTIALIAS_RGBA, HINT_FULL);

    group = enum_group_create (data->font_settings, FONT_HINTING_KEY,
                               appearance_capplet_get_widget (data, "hint_none_radio"),   HINT_NONE,
                               appearance_capplet_get_widget (data, "hint_slight_radio"), HINT_SLIGHT,
                               appearance_capplet_get_widget (data, "hint_medium_radio"), HINT_MEDIUM,
                               appearance_capplet_get_widget (data, "hint_full_radio"),   HINT_FULL,
                               NULL);
    data->font_groups = g_slist_prepend (data->font_groups, group);

    ctk_image_set_from_file (CTK_IMAGE (appearance_capplet_get_widget (data, "subpixel_rgb_image")),
                             CAFECC_PIXMAP_DIR "/subpixel-rgb.png");
    ctk_image_set_from_file (CTK_IMAGE (appearance_capplet_get_widget (data, "subpixel_bgr_image")),
                             CAFECC_PIXMAP_DIR "/subpixel-bgr.png");
    ctk_image_set_from_file (CTK_IMAGE (appearance_capplet_get_widget (data, "subpixel_vrgb_image")),
                             CAFECC_PIXMAP_DIR "/subpixel-vrgb.png");
    ctk_image_set_from_file (CTK_IMAGE (appearance_capplet_get_widget (data, "subpixel_vbgr_image")),
                             CAFECC_PIXMAP_DIR "/subpixel-vbgr.png");

    group = enum_group_create (data->font_settings, FONT_RGBA_ORDER_KEY,
                               appearance_capplet_get_widget (data, "subpixel_rgb_radio"),  RGBA_RGB,
                               appearance_capplet_get_widget (data, "subpixel_bgr_radio"),  RGBA_BGR,
                               appearance_capplet_get_widget (data, "subpixel_vrgb_radio"), RGBA_VRGB,
                               appearance_capplet_get_widget (data, "subpixel_vbgr_radio"), RGBA_VBGR,
                               NULL);
    data->font_groups = g_slist_prepend (data->font_groups, group);

    g_signal_connect (G_OBJECT (data->font_details),
		      "response",
		      G_CALLBACK (cb_details_response), NULL);
    g_signal_connect (G_OBJECT (data->font_details),
		      "delete_event",
		      G_CALLBACK (ctk_true), NULL);
  }

  ctk_window_present (CTK_WINDOW (data->font_details));
}

void font_init(AppearanceData* data)
{
	CtkWidget* widget;

	data->font_details = NULL;
	data->font_groups = NULL;

	widget = appearance_capplet_get_widget(data, "application_font");
	g_settings_bind (data->interface_settings,
			 CTK_FONT_KEY,
			 G_OBJECT (widget),
			 "font-name",
			 G_SETTINGS_BIND_DEFAULT);

	widget = appearance_capplet_get_widget (data, "document_font");
	g_settings_bind (data->interface_settings,
			 DOCUMENT_FONT_KEY,
			 G_OBJECT (widget),
			 "font-name",
			 G_SETTINGS_BIND_DEFAULT);

	widget = appearance_capplet_get_widget (data, "desktop_font");

	if (data->caja_settings)
		g_settings_bind (data->caja_settings,
				 DESKTOP_FONT_KEY,
				 G_OBJECT (widget),
				 "font-name",
				 G_SETTINGS_BIND_DEFAULT);
	else
		ctk_widget_set_sensitive (widget, FALSE);

	widget = appearance_capplet_get_widget (data, "window_title_font");
	g_settings_bind (data->croma_settings,
			 WINDOW_TITLE_FONT_KEY,
			 G_OBJECT (widget),
			 "font-name",
			 G_SETTINGS_BIND_DEFAULT);

	widget = appearance_capplet_get_widget (data, "monospace_font");
	g_settings_bind (data->interface_settings,
			 MONOSPACE_FONT_KEY,
			 G_OBJECT (widget),
			 "font-name",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (data->croma_settings,
			  "changed::" WINDOW_TITLE_USES_SYSTEM_KEY,
			  G_CALLBACK (croma_changed),
			  data);

	g_signal_connect (appearance_capplet_get_widget (data, "add_new_font"), "clicked", G_CALLBACK (cb_add_new_font), data);

	croma_titlebar_load_sensitivity(data);

	setup_font_pair(appearance_capplet_get_widget(data, "monochrome_radio"), appearance_capplet_get_widget (data, "monochrome_sample"), ANTIALIAS_NONE, HINT_FULL);
	setup_font_pair(appearance_capplet_get_widget(data, "best_shapes_radio"), appearance_capplet_get_widget (data, "best_shapes_sample"), ANTIALIAS_GRAYSCALE, HINT_MEDIUM);
	setup_font_pair(appearance_capplet_get_widget(data, "best_contrast_radio"), appearance_capplet_get_widget (data, "best_contrast_sample"), ANTIALIAS_GRAYSCALE, HINT_FULL);
	setup_font_pair(appearance_capplet_get_widget(data, "subpixel_radio"), appearance_capplet_get_widget (data, "subpixel_sample"), ANTIALIAS_RGBA, HINT_SLIGHT);

	font_render_load (data->font_settings);

	g_signal_connect (data->font_settings, "changed", G_CALLBACK (font_render_changed), NULL);

	g_signal_connect (appearance_capplet_get_widget (data, "details_button"), "clicked", G_CALLBACK (cb_show_details), data);
}

void font_shutdown(AppearanceData* data)
{
	g_slist_foreach(data->font_groups, (GFunc) enum_group_destroy, NULL);
	g_slist_free(data->font_groups);
	g_slist_foreach(font_pairs, (GFunc) g_free, NULL);
	g_slist_free(font_pairs);
}
