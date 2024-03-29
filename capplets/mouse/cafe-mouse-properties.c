/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
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

#include <config.h>

#include <glib/gi18n.h>
#include <string.h>
#include <gio/gio.h>
#include <cdk/cdkx.h>
#include <math.h>

#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "csd_input-helper.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON
};

typedef enum {
	ACCEL_PROFILE_DEFAULT,
	ACCEL_PROFILE_ADAPTIVE,
	ACCEL_PROFILE_FLAT
} AccelProfile;

#define MOUSE_SCHEMA "org.cafe.peripherals-mouse"
#define INTERFACE_SCHEMA "org.cafe.interface"
#define DOUBLE_CLICK_KEY "double-click"

#define TOUCHPAD_SCHEMA "org.cafe.peripherals-touchpad"

/* State in testing the double-click speed. Global for a great deal of
 * convenience
 */
static gint double_click_state = DOUBLE_CLICK_TEST_OFF;

static GSettings *mouse_settings = NULL;
static GSettings *interface_settings = NULL;
static GSettings *touchpad_settings = NULL;

/* Double Click handling */

struct test_data_t
{
	gint *timeout_id;
	CtkWidget *image;
};

/* Timeout for the double click test */

static gboolean
test_maybe_timeout (struct test_data_t *data)
{
	double_click_state = DOUBLE_CLICK_TEST_OFF;

	ctk_image_set_from_resource (CTK_IMAGE (data->image), "/org/cafe/ccc/mouse/double-click-off.png");

	*data->timeout_id = 0;

	return FALSE;
}

/* Callback issued when the user clicks the double click testing area. */

static gboolean
event_box_button_press_event (CtkWidget   *widget,
			      CdkEventButton *event,
			      gpointer user_data)
{
	gint                       double_click_time;
	static struct test_data_t  data;
	static gint                test_on_timeout_id     = 0;
	static gint                test_maybe_timeout_id  = 0;
	static guint32             double_click_timestamp = 0;
	CtkWidget                 *image;

	if (event->type != CDK_BUTTON_PRESS)
		return FALSE;

	image = g_object_get_data (G_OBJECT (widget), "image");

	double_click_time = g_settings_get_int (mouse_settings, DOUBLE_CLICK_KEY);

	if (test_maybe_timeout_id != 0) {
		g_source_remove  (test_maybe_timeout_id);
		test_maybe_timeout_id = 0;
	}
	if (test_on_timeout_id != 0) {
		g_source_remove (test_on_timeout_id);
		test_on_timeout_id = 0;
	}

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		data.image = image;
		data.timeout_id = &test_maybe_timeout_id;
		test_maybe_timeout_id = g_timeout_add (double_click_time, (GSourceFunc) test_maybe_timeout, &data);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state = DOUBLE_CLICK_TEST_ON;
			data.image = image;
			data.timeout_id = &test_on_timeout_id;
			test_on_timeout_id = g_timeout_add (2500, (GSourceFunc) test_maybe_timeout, &data);
		}
		break;
	case DOUBLE_CLICK_TEST_ON:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_ON:
		ctk_image_set_from_resource (CTK_IMAGE (image), "/org/cafe/ccc/mouse/double-click-on.png");
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		ctk_image_set_from_resource (CTK_IMAGE (image), "/org/cafe/ccc/mouse/double-click-maybe.png");
		break;
	case DOUBLE_CLICK_TEST_OFF:
		ctk_image_set_from_resource (CTK_IMAGE (image), "/org/cafe/ccc/mouse/double-click-off.png");
		break;
	}

	return TRUE;
}

static void
orientation_radio_button_release_event (CtkWidget   *widget,
				        CdkEventButton *event)
{
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
orientation_radio_button_toggled (CtkToggleButton *togglebutton,
				        CtkBuilder *dialog)
{
	gboolean left_handed;
	left_handed = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (WID ("left_handed_radio")));
	g_settings_set_boolean (mouse_settings, "left-handed", left_handed);
}

static void
synaptics_check_capabilities (CtkBuilder *dialog)
{
	CdkDisplay *display;
	int numdevices, i;
	XDeviceInfo *devicelist;
	Atom realtype, prop;
	int realformat;
	unsigned long nitems, bytes_after;
	unsigned char *data;

	display = cdk_display_get_default ();
	prop = XInternAtom (CDK_DISPLAY_XDISPLAY(display), "Synaptics Capabilities", True);
	if (!prop)
		return;

	devicelist = XListInputDevices (CDK_DISPLAY_XDISPLAY(display), &numdevices);
	for (i = 0; i < numdevices; i++) {
		if (devicelist[i].use != IsXExtensionPointer)
			continue;

		cdk_x11_display_error_trap_push (display);
		XDevice *device = XOpenDevice (CDK_DISPLAY_XDISPLAY(display),
					       devicelist[i].id);
		if (cdk_x11_display_error_trap_pop (display))
			continue;

		cdk_x11_display_error_trap_push (display);
		if ((XGetDeviceProperty (CDK_DISPLAY_XDISPLAY(display), device, prop, 0, 2, False,
					 XA_INTEGER, &realtype, &realformat, &nitems,
					 &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is booleans for has_left, has_middle,
			 * has_right, has_double, has_triple */
			if (!data[0]) {
				ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (WID ("tap_to_click_toggle")), TRUE);
				ctk_widget_set_sensitive (WID ("tap_to_click_toggle"), FALSE);
			}

			XFree (data);
		}

		cdk_x11_display_error_trap_pop_ignored (display);

		XCloseDevice (CDK_DISPLAY_XDISPLAY(display), device);
	}
	XFreeDeviceList (devicelist);
}

static void
accel_profile_combobox_changed_callback (CtkWidget *combobox, void *data)
{
	AccelProfile value = ctk_combo_box_get_active (CTK_COMBO_BOX (combobox));
	g_settings_set_enum (mouse_settings, (const gchar *) "accel-profile", value);
}

static void
comboxbox_changed (CtkWidget *combobox, CtkBuilder *dialog, const char *key)
{
	gint value = ctk_combo_box_get_active (CTK_COMBO_BOX (combobox));
	gint value2, value3;
	CtkLabel *warn = CTK_LABEL (WID ("multi_finger_warning"));

	g_settings_set_int (touchpad_settings, (const gchar *) key, value);

	/* Show warning if some multi-finger click emulation is enabled. */
	value2 = g_settings_get_int (touchpad_settings, "two-finger-click");
	value3 = g_settings_get_int (touchpad_settings, "three-finger-click");
	ctk_widget_set_opacity (CTK_WIDGET (warn), (value2 || value3)?  1.0: 0.0);
}

static void
comboxbox_two_finger_changed_callback (CtkWidget *combobox, void *data)
{
	comboxbox_changed (combobox, CTK_BUILDER (data), "two-finger-click");
}

static void
comboxbox_three_finger_changed_callback (CtkWidget *combobox, void *data)
{
	comboxbox_changed (combobox, CTK_BUILDER (data), "three-finger-click");
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CtkBuilder *dialog)
{
	CtkRadioButton    *radio;

	/* Orientation radio buttons */
	radio = CTK_RADIO_BUTTON (WID ("left_handed_radio"));
	ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON(radio),
		g_settings_get_boolean(mouse_settings, "left-handed"));
	/* explicitly connect to button-release so that you can change orientation with either button */
	g_signal_connect (WID ("right_handed_radio"), "button_release_event",
		G_CALLBACK (orientation_radio_button_release_event), NULL);
	g_signal_connect (WID ("left_handed_radio"), "button_release_event",
		G_CALLBACK (orientation_radio_button_release_event), NULL);
	g_signal_connect (WID ("left_handed_radio"), "toggled",
		G_CALLBACK (orientation_radio_button_toggled), dialog);

	/* Locate pointer toggle */
	g_settings_bind (mouse_settings, "locate-pointer", WID ("locate_pointer_toggle"),
		"active", G_SETTINGS_BIND_DEFAULT);

	/* Middle Button Emulation */
	g_settings_bind (mouse_settings, "middle-button-enabled", WID ("middle_button_emulation_toggle"),
		"active", G_SETTINGS_BIND_DEFAULT);

	/* Middle Button Paste */
	g_settings_bind (interface_settings, "ctk-enable-primary-paste", WID ("middle_button_paste_toggle"),
		"active", G_SETTINGS_BIND_DEFAULT);


	/* Double-click time */
	g_settings_bind (mouse_settings, DOUBLE_CLICK_KEY,
		ctk_range_get_adjustment (CTK_RANGE (WID ("delay_scale"))), "value",
		G_SETTINGS_BIND_DEFAULT);

	ctk_image_set_from_resource (CTK_IMAGE (WID ("double_click_image")), "/org/cafe/ccc/mouse/double-click-off.png");
	g_object_set_data (G_OBJECT (WID ("double_click_eventbox")), "image", WID ("double_click_image"));
	g_signal_connect (WID ("double_click_eventbox"), "button_press_event",
			  G_CALLBACK (event_box_button_press_event), NULL);

	/* speed */
	g_settings_bind (mouse_settings, "motion-acceleration",
		ctk_range_get_adjustment (CTK_RANGE (WID ("accel_scale"))), "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (mouse_settings, "motion-threshold",
		ctk_range_get_adjustment (CTK_RANGE (WID ("sensitivity_scale"))), "value",
		G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (WID ("mouse_accel_profile"), "changed",
			  G_CALLBACK (accel_profile_combobox_changed_callback), NULL);
	ctk_combo_box_set_active (CTK_COMBO_BOX (WID ("mouse_accel_profile")),
				  g_settings_get_enum (mouse_settings, "accel-profile"));

	/* DnD threshold */
	g_settings_bind (mouse_settings, "drag-threshold",
		ctk_range_get_adjustment (CTK_RANGE (WID ("drag_threshold_scale"))), "value",
		G_SETTINGS_BIND_DEFAULT);

	/* Trackpad page */
	if (touchpad_is_present () == FALSE)
		ctk_notebook_remove_page (CTK_NOTEBOOK (WID ("prefs_widget")), -1);
	else {
		g_settings_bind (touchpad_settings, "touchpad-enabled",
			WID ("touchpad_enable"), "active",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "touchpad-enabled",
			WID ("vbox_touchpad_general"), "sensitive",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "touchpad-enabled",
			WID ("vbox_touchpad_scrolling"), "sensitive",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "touchpad-enabled",
			WID ("vbox_touchpad_pointer_speed"), "sensitive",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "disable-while-typing",
			WID ("disable_w_typing_toggle"), "active",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "tap-to-click",
			WID ("tap_to_click_toggle"), "active",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "vertical-edge-scrolling", WID ("vert_edge_scroll_toggle"), "active", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "horizontal-edge-scrolling", WID ("horiz_edge_scroll_toggle"), "active", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "vertical-two-finger-scrolling", WID ("vert_twofinger_scroll_toggle"), "active", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "horizontal-two-finger-scrolling", WID ("horiz_twofinger_scroll_toggle"), "active", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "natural-scroll", WID ("natural_scroll_toggle"), "active", G_SETTINGS_BIND_DEFAULT);

		char * emulation_values[] = { _("Disabled"), _("Left button"), _("Middle button"), _("Right button") };

		CtkWidget *two_click_comboxbox = ctk_combo_box_text_new ();
		CtkWidget *three_click_comboxbox = ctk_combo_box_text_new ();
		ctk_box_pack_start (CTK_BOX (WID ("hbox_two_finger_click")), two_click_comboxbox, FALSE, FALSE, 6);
		ctk_box_pack_start (CTK_BOX (WID ("hbox_three_finger_click")), three_click_comboxbox, FALSE, FALSE, 6);
		int i;
		for (i=0; i<4; i++) {
			ctk_combo_box_text_append_text (CTK_COMBO_BOX_TEXT (two_click_comboxbox), emulation_values[i]);
			ctk_combo_box_text_append_text (CTK_COMBO_BOX_TEXT (three_click_comboxbox), emulation_values[i]);
		}

		g_signal_connect (two_click_comboxbox, "changed", G_CALLBACK (comboxbox_two_finger_changed_callback), dialog);
		g_signal_connect (three_click_comboxbox, "changed", G_CALLBACK (comboxbox_three_finger_changed_callback), dialog);
		ctk_combo_box_set_active (CTK_COMBO_BOX (two_click_comboxbox), g_settings_get_int (touchpad_settings, "two-finger-click"));
		ctk_combo_box_set_active (CTK_COMBO_BOX (three_click_comboxbox), g_settings_get_int (touchpad_settings, "three-finger-click"));
		ctk_widget_show (two_click_comboxbox);
		ctk_widget_show (three_click_comboxbox);

		/* speed */
		g_settings_bind (touchpad_settings, "motion-acceleration",
			ctk_range_get_adjustment (CTK_RANGE (WID ("touchpad_accel_scale"))), "value",
			G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "motion-threshold",
			ctk_range_get_adjustment (CTK_RANGE (WID ("touchpad_sensitivity_scale"))), "value",
			G_SETTINGS_BIND_DEFAULT);

		synaptics_check_capabilities (dialog);
	}

}

/* Construct the dialog */

static CtkBuilder *
create_dialog (void)
{
	CtkBuilder   *dialog;
	CtkSizeGroup *size_group;
	GError       *error = NULL;

	dialog = ctk_builder_new ();
	if (ctk_builder_add_from_resource (dialog, "/org/cafe/ccc/mouse/cafe-mouse-properties.ui", &error) == 0) {
		g_warning ("Error loading UI file: %s", error->message);
		g_error_free (error);
		g_object_unref (dialog);
		return NULL;
	}

	size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
	ctk_size_group_add_widget (size_group, WID ("acceleration_label"));
	ctk_size_group_add_widget (size_group, WID ("sensitivity_label"));
	ctk_size_group_add_widget (size_group, WID ("threshold_label"));
	ctk_size_group_add_widget (size_group, WID ("timeout_label"));

	size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
	ctk_size_group_add_widget (size_group, WID ("acceleration_fast_label"));
	ctk_size_group_add_widget (size_group, WID ("sensitivity_high_label"));
	ctk_size_group_add_widget (size_group, WID ("threshold_large_label"));
	ctk_size_group_add_widget (size_group, WID ("timeout_long_label"));

	size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
	ctk_size_group_add_widget (size_group, WID ("acceleration_slow_label"));
	ctk_size_group_add_widget (size_group, WID ("sensitivity_low_label"));
	ctk_size_group_add_widget (size_group, WID ("threshold_small_label"));
	ctk_size_group_add_widget (size_group, WID ("timeout_short_label"));

	return dialog;
}

/* Callback issued when a button is clicked on the dialog */

static void
dialog_response_cb (CtkDialog *dialog, gint response_id, gpointer data)
{
	if (response_id == CTK_RESPONSE_HELP)
		capplet_help (CTK_WINDOW (dialog),
			      "goscustperiph-5");
	else
		ctk_main_quit ();
}

int
main (int argc, char **argv)
{
	CtkBuilder     *dialog;
	CtkWidget      *dialog_win, *w;
	gchar *start_page = NULL;

	GOptionContext *context;
	GOptionEntry cap_options[] = {
		{"show-page", 'p', G_OPTION_FLAG_IN_MAIN,
		 G_OPTION_ARG_STRING,
		 &start_page,
		 /* TRANSLATORS: don't translate the terms in brackets */
		 N_("Specify the name of the page to show (general)"),
		 N_("page") },
		{NULL}
	};

	context = g_option_context_new (_("- CAFE Mouse Preferences"));
	g_option_context_add_main_entries (context, cap_options, GETTEXT_PACKAGE);
	capplet_init (context, &argc, &argv);

	activate_settings_daemon ();

	mouse_settings = g_settings_new (MOUSE_SCHEMA);
	interface_settings = g_settings_new (INTERFACE_SCHEMA);
	touchpad_settings = g_settings_new (TOUCHPAD_SCHEMA);

	dialog = create_dialog ();

	if (dialog) {
		setup_dialog (dialog);

		dialog_win = WID ("mouse_properties_dialog");
		g_signal_connect (dialog_win, "response",
				  G_CALLBACK (dialog_response_cb), NULL);

                CtkNotebook* nb = CTK_NOTEBOOK (WID ("prefs_widget"));
                ctk_widget_add_events (CTK_WIDGET (nb), CDK_SCROLL_MASK);
                g_signal_connect (CTK_WIDGET (nb), "scroll-event",
                                  G_CALLBACK (capplet_dialog_page_scroll_event_cb),
                                  CTK_WINDOW (dialog_win));


		if (start_page != NULL) {
			gchar *page_name;

			page_name = g_strconcat (start_page, "_vbox", NULL);
			g_free (start_page);

			w = WID (page_name);
			if (w != NULL) {
				gint pindex;

				pindex = ctk_notebook_page_num (nb, w);
				if (pindex != -1)
					ctk_notebook_set_current_page (nb, pindex);
			}
			g_free (page_name);
		}

		capplet_set_icon (dialog_win, "input-mouse");
		ctk_widget_show (dialog_win);

		g_object_set (ctk_settings_get_default (), "ctk-button-images", TRUE, NULL);

		ctk_main ();

		g_object_unref (dialog);
	}

	g_object_unref (mouse_settings);
	g_object_unref (interface_settings);
	g_object_unref (touchpad_settings);

	return 0;
}
