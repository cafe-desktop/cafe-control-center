/* cafe-about-me-fingerprint.h
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
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

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <dbus/dbus-glib-bindings.h>

#include "fingerprint-strings.h"
#include "capplet-util.h"

/* This must match the number of images on the 2nd page in the UI file */
#define MAX_ENROLL_STAGES 5

/* Translate fprintd strings */
#define TR(s) dgettext("fprintd", s)

static DBusGProxy *manager = NULL;
static DBusGConnection *connection = NULL;
static gboolean is_disable = FALSE;

enum {
	STATE_NONE,
	STATE_CLAIMED,
	STATE_ENROLLING
};

typedef struct {
	CtkWidget *enable;
	CtkWidget *disable;

	CtkWidget *ass;
	CtkBuilder *dialog;

	DBusGProxy *device;
	gboolean is_swipe;
	int num_enroll_stages;
	int num_stages_done;
	char *name;
	const char *finger;
	gint state;
} EnrollData;

static void create_manager (void)
{
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (connection == NULL) {
		g_warning ("Failed to connect to session bus: %s", error->message);
		return;
	}

	manager = dbus_g_proxy_new_for_name (connection,
					     "net.reactivated.Fprint",
					     "/net/reactivated/Fprint/Manager",
					     "net.reactivated.Fprint.Manager");
}

static DBusGProxy *
get_first_device (void)
{
	DBusGProxy *device;
	char *device_str;

	if (!dbus_g_proxy_call (manager, "GetDefaultDevice", NULL, G_TYPE_INVALID,
				DBUS_TYPE_G_OBJECT_PATH, &device_str, G_TYPE_INVALID)) {
		return NULL;
	}

	device = dbus_g_proxy_new_for_name(connection,
					   "net.reactivated.Fprint",
					   device_str,
					   "net.reactivated.Fprint.Device");

	g_free (device_str);

	return device;
}

static const char *
get_reason_for_error (const char *dbus_error)
{
	if (g_str_equal (dbus_error, "net.reactivated.Fprint.Error.PermissionDenied"))
		return N_("You are not allowed to access the device. Contact your system administrator.");
	if (g_str_equal (dbus_error, "net.reactivated.Fprint.Error.AlreadyInUse"))
		return N_("The device is already in use.");
	if (g_str_equal (dbus_error, "net.reactivated.Fprint.Error.Internal"))
		return N_("An internal error occurred");

	return NULL;
}

static CtkWidget *
get_error_dialog (const char *title,
		  const char *dbus_error,
		  CtkWindow *parent)
{
	CtkWidget *error_dialog;
	const char *reason;

	if (dbus_error == NULL)
		g_warning ("get_error_dialog called with reason == NULL");

	error_dialog =
		ctk_message_dialog_new (parent,
				CTK_DIALOG_MODAL,
				CTK_MESSAGE_ERROR,
				CTK_BUTTONS_OK,
				"%s", title);
	reason = get_reason_for_error (dbus_error);
	ctk_message_dialog_format_secondary_text
		(CTK_MESSAGE_DIALOG (error_dialog), "%s", reason ? _(reason) : _(dbus_error));

	ctk_window_set_title (CTK_WINDOW (error_dialog), ""); /* as per HIG */
	ctk_container_set_border_width (CTK_CONTAINER (error_dialog), 5);
	ctk_dialog_set_default_response (CTK_DIALOG (error_dialog),
					 CTK_RESPONSE_OK);
	ctk_window_set_modal (CTK_WINDOW (error_dialog), TRUE);
	ctk_window_set_position (CTK_WINDOW (error_dialog), CTK_WIN_POS_CENTER_ON_PARENT);

	return error_dialog;
}

void
set_fingerprint_label (CtkWidget *enable, CtkWidget *disable)
{
	char **fingers;
	DBusGProxy *device;
	GError *error = NULL;

	ctk_widget_set_no_show_all (enable, TRUE);
	ctk_widget_set_no_show_all (disable, TRUE);

	if (manager == NULL) {
		create_manager ();
		if (manager == NULL) {
			ctk_widget_hide (enable);
			ctk_widget_hide (disable);
			return;
		}
	}

	device = get_first_device ();
	if (device == NULL) {
		ctk_widget_hide (enable);
		ctk_widget_hide (disable);
		return;
	}

	if (!dbus_g_proxy_call (device, "ListEnrolledFingers", &error, G_TYPE_STRING, "", G_TYPE_INVALID,
				G_TYPE_STRV, &fingers, G_TYPE_INVALID)) {
		if (dbus_g_error_has_name (error, "net.reactivated.Fprint.Error.NoEnrolledPrints") == FALSE) {
			ctk_widget_hide (enable);
			ctk_widget_hide (disable);
			g_object_unref (device);
			return;
		}
		fingers = NULL;
	}

	if (fingers == NULL || g_strv_length (fingers) == 0) {
		ctk_widget_hide (disable);
		ctk_widget_show (enable);
		is_disable = FALSE;
	} else {
		ctk_widget_hide (enable);
		ctk_widget_show (disable);
		is_disable = TRUE;
	}

	g_strfreev (fingers);
	g_object_unref (device);
}

static void
delete_fingerprints (void)
{
	DBusGProxy *device;

	if (manager == NULL) {
		create_manager ();
		if (manager == NULL)
			return;
	}

	device = get_first_device ();
	if (device == NULL)
		return;

	dbus_g_proxy_call (device, "DeleteEnrolledFingers", NULL, G_TYPE_STRING, "", G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref (device);
}

static void
delete_fingerprints_question (CtkBuilder *dialog, CtkWidget *enable, CtkWidget *disable)
{
	CtkWidget *question;
	CtkWidget *button;

	question = ctk_message_dialog_new (CTK_WINDOW (WID ("about-me-dialog")),
					   CTK_DIALOG_MODAL,
					   CTK_MESSAGE_QUESTION,
					   CTK_BUTTONS_NONE,
					   _("Delete registered fingerprints?"));
	ctk_dialog_add_button (CTK_DIALOG (question), "ctk-cancel", CTK_RESPONSE_CANCEL);

	button = ctk_button_new_with_mnemonic (_("_Delete Fingerprints"));
	ctk_button_set_image (CTK_BUTTON (button), ctk_image_new_from_icon_name ("edit-delete", CTK_ICON_SIZE_BUTTON));
	ctk_widget_set_can_default (button, TRUE);
	ctk_widget_show (button);
	ctk_dialog_add_action_widget (CTK_DIALOG (question), button, CTK_RESPONSE_OK);

	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (question),
						  _("Do you want to delete your registered fingerprints so fingerprint login is disabled?"));
	ctk_container_set_border_width (CTK_CONTAINER (question), 5);
	ctk_dialog_set_default_response (CTK_DIALOG (question), CTK_RESPONSE_OK);
	ctk_window_set_position (CTK_WINDOW (question), CTK_WIN_POS_CENTER_ON_PARENT);
	ctk_window_set_modal (CTK_WINDOW (question), TRUE);

	if (ctk_dialog_run (CTK_DIALOG (question)) == CTK_RESPONSE_OK) {
		delete_fingerprints ();
		set_fingerprint_label (enable, disable);
	}

	ctk_widget_destroy (question);
}

static void
enroll_data_destroy (EnrollData *data)
{
	switch (data->state) {
	case STATE_ENROLLING:
		dbus_g_proxy_call(data->device, "EnrollStop", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
		/* fall-through */
	case STATE_CLAIMED:
		dbus_g_proxy_call(data->device, "Release", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
		/* fall-through */
	case STATE_NONE:
		g_free (data->name);
		g_object_unref (data->device);
		g_object_unref (data->dialog);
		ctk_widget_destroy (data->ass);

		g_free (data);
	}
}

static const char *
selected_finger (CtkBuilder *dialog)
{
	int index;

	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (WID ("radiobutton1")))) {
		ctk_widget_set_sensitive (WID ("finger_combobox"), FALSE);
		return "right-index-finger";
	}
	if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (WID ("radiobutton2")))) {
		ctk_widget_set_sensitive (WID ("finger_combobox"), FALSE);
		return "left-index-finger";
	}
	ctk_widget_set_sensitive (WID ("finger_combobox"), TRUE);
	index = ctk_combo_box_get_active (CTK_COMBO_BOX (WID ("finger_combobox")));
	switch (index) {
	case 0:
		return "left-thumb";
	case 1:
		return "left-middle-finger";
	case 2:
		return "left-ring-finger";
	case 3:
		return "left-little-finger";
	case 4:
		return "right-thumb";
	case 5:
		return "right-middle-finger";
	case 6:
		return "right-ring-finger";
	case 7:
		return "right-little-finger";
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static void
finger_radio_button_toggled (CtkToggleButton *button, EnrollData *data)
{
	CtkBuilder *dialog = data->dialog;
	char *msg;

	data->finger = selected_finger (data->dialog);

	msg = g_strdup_printf (TR(finger_str_to_msg (data->finger, data->is_swipe)), data->name);
	ctk_label_set_text (CTK_LABEL (WID("enroll-label")), msg);
	g_free (msg);
}

static void
finger_combobox_changed (CtkComboBox *combobox, EnrollData *data)
{
	CtkBuilder *dialog = data->dialog;
	char *msg;

	data->finger = selected_finger (data->dialog);

	msg = g_strdup_printf (TR(finger_str_to_msg (data->finger, data->is_swipe)), data->name);
	ctk_label_set_text (CTK_LABEL (WID("enroll-label")), msg);
	g_free (msg);
}

static void
assistant_cancelled (CtkAssistant *ass, EnrollData *data)
{
	CtkWidget *enable, *disable;

	enable = data->enable;
	disable = data->disable;

	enroll_data_destroy (data);
	set_fingerprint_label (enable, disable);
}

static void
enroll_result (GObject *object, const char *result, gboolean done, EnrollData *data)
{
	CtkBuilder *dialog = data->dialog;
	char *msg;

	if (g_str_equal (result, "enroll-completed") || g_str_equal (result, "enroll-stage-passed")) {
		char *name, *path;

		data->num_stages_done++;
		name = g_strdup_printf ("image%d", data->num_stages_done);
		path = g_build_filename (CAFECC_PIXMAP_DIR, "print_ok.png", NULL);
		ctk_image_set_from_file (CTK_IMAGE (WID (name)), path);
		g_free (name);
		g_free (path);
	}
	if (g_str_equal (result, "enroll-completed")) {
		ctk_label_set_text (CTK_LABEL (WID ("status-label")), _("Done!"));
		ctk_assistant_set_page_complete (CTK_ASSISTANT (data->ass), WID ("page2"), TRUE);
	}

	if (done != FALSE) {
		dbus_g_proxy_call(data->device, "EnrollStop", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
		data->state = STATE_CLAIMED;
		if (g_str_equal (result, "enroll-completed") == FALSE) {
			/* The enrollment failed, restart it */
			dbus_g_proxy_call(data->device, "EnrollStart", NULL, G_TYPE_STRING, data->finger, G_TYPE_INVALID, G_TYPE_INVALID);
			data->state = STATE_ENROLLING;
			result = "enroll-retry-scan";
		} else {
			return;
		}
	}

	msg = g_strdup_printf (TR(enroll_result_str_to_msg (result, data->is_swipe)), data->name);
	ctk_label_set_text (CTK_LABEL (WID ("status-label")), msg);
	g_free (msg);
}

static void
assistant_prepare (CtkAssistant *ass, CtkWidget *page, EnrollData *data)
{
	const char *name;

	name = g_object_get_data (G_OBJECT (page), "name");
	if (name == NULL)
		return;

	if (g_str_equal (name, "enroll")) {
		DBusGProxy *p;
		GError *error = NULL;
		CtkBuilder *dialog = data->dialog;
		char *path;
		guint i;
		GValue value = { 0, };

		if (!dbus_g_proxy_call (data->device, "Claim", &error, G_TYPE_STRING, "", G_TYPE_INVALID, G_TYPE_INVALID)) {
			CtkWidget *d;
			char *msg;

			/* translators:
			 * The variable is the name of the device, for example:
			 * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
			msg = g_strdup_printf (_("Could not access '%s' device"), data->name);
			d = get_error_dialog (msg, dbus_g_error_get_name (error), CTK_WINDOW (data->ass));
			g_error_free (error);
			ctk_dialog_run (CTK_DIALOG (d));
			ctk_widget_destroy (d);
			g_free (msg);

			enroll_data_destroy (data);

			return;
		}
		data->state = STATE_CLAIMED;

		p = dbus_g_proxy_new_from_proxy (data->device, "org.freedesktop.DBus.Properties", NULL);
		if (!dbus_g_proxy_call (p, "Get", NULL, G_TYPE_STRING, "net.reactivated.Fprint.Device", G_TYPE_STRING, "num-enroll-stages", G_TYPE_INVALID,
				       G_TYPE_VALUE, &value, G_TYPE_INVALID) || g_value_get_int (&value) < 1) {
			CtkWidget *d;
			char *msg;

			/* translators:
			 * The variable is the name of the device, for example:
			 * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
			msg = g_strdup_printf (_("Could not access '%s' device"), data->name);
			d = get_error_dialog (msg, "net.reactivated.Fprint.Error.Internal", CTK_WINDOW (data->ass));
			ctk_dialog_run (CTK_DIALOG (d));
			ctk_widget_destroy (d);
			g_free (msg);

			enroll_data_destroy (data);

			g_object_unref (p);
			return;
		}
		g_object_unref (p);

		data->num_enroll_stages = g_value_get_int (&value);

		/* Hide the extra "bulbs" if not needed */
		for (i = MAX_ENROLL_STAGES; i > data->num_enroll_stages; i--) {
			char *name;

			name = g_strdup_printf ("image%d", i);
			ctk_widget_hide (WID (name));
			g_free (name);
		}
		/* And set the right image */
		{
			char *filename;

			filename = g_strdup_printf ("%s.png", data->finger);
			path = g_build_filename (CAFECC_PIXMAP_DIR, filename, NULL);
			g_free (filename);
		}
		for (i = 1; i <= data->num_enroll_stages; i++) {
			char *name;
			name = g_strdup_printf ("image%d", i);
			ctk_image_set_from_file (CTK_IMAGE (WID (name)), path);
			g_free (name);
		}
		g_free (path);

		dbus_g_proxy_add_signal(data->device, "EnrollStatus", G_TYPE_STRING, G_TYPE_BOOLEAN, NULL);
		dbus_g_proxy_connect_signal(data->device, "EnrollStatus", G_CALLBACK(enroll_result), data, NULL);

		if (!dbus_g_proxy_call(data->device, "EnrollStart", &error, G_TYPE_STRING, data->finger, G_TYPE_INVALID, G_TYPE_INVALID)) {
			CtkWidget *d;
			char *msg;

			/* translators:
			 * The variable is the name of the device, for example:
			 * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
			msg = g_strdup_printf (_("Could not start finger capture on '%s' device"), data->name);
			d = get_error_dialog (msg, dbus_g_error_get_name (error), CTK_WINDOW (data->ass));
			g_error_free (error);
			ctk_dialog_run (CTK_DIALOG (d));
			ctk_widget_destroy (d);
			g_free (msg);

			enroll_data_destroy (data);

			return;
		}
		data->state = STATE_ENROLLING;;
	} else {
		if (data->state == STATE_ENROLLING) {
			dbus_g_proxy_call(data->device, "EnrollStop", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
			data->state = STATE_CLAIMED;
		}
		if (data->state == STATE_CLAIMED) {
			dbus_g_proxy_call(data->device, "Release", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
			data->state = STATE_NONE;
		}
	}
}

static void
enroll_fingerprints (CtkWindow *parent, CtkWidget *enable, CtkWidget *disable)
{
	DBusGProxy *device, *p;
	GHashTable *props;
	CtkBuilder *dialog;
	EnrollData *data;
	CtkWidget *ass;
	char *msg;
	GError *error = NULL;

	device = NULL;

	if (manager == NULL) {
		create_manager ();
		if (manager != NULL)
			device = get_first_device ();
	} else {
		device = get_first_device ();
	}

	if (manager == NULL || device == NULL) {
		CtkWidget *d;

		d = get_error_dialog (_("Could not access any fingerprint readers"),
				      _("Please contact your system administrator for help."),
				      parent);
		ctk_dialog_run (CTK_DIALOG (d));
		ctk_widget_destroy (d);
		return;
	}

	data = g_new0 (EnrollData, 1);
	data->device = device;
	data->enable = enable;
	data->disable = disable;

	/* Get some details about the device */
	p = dbus_g_proxy_new_from_proxy (device, "org.freedesktop.DBus.Properties", NULL);
	if (dbus_g_proxy_call (p, "GetAll", NULL, G_TYPE_STRING, "net.reactivated.Fprint.Device", G_TYPE_INVALID,
			       dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props, G_TYPE_INVALID)) {
		const char *scan_type;
		data->name = g_value_dup_string (g_hash_table_lookup (props, "name"));
		scan_type = g_value_dup_string (g_hash_table_lookup (props, "scan-type"));
		if (g_str_equal (scan_type, "swipe"))
			data->is_swipe = TRUE;
		g_hash_table_destroy (props);
	}
	g_object_unref (p);

	dialog = ctk_builder_new ();
	if (ctk_builder_add_from_resource (dialog, "/org/cafe/ccc/am/cafe-about-me-fingerprint.ui", &error) == 0)
	{
		g_warning ("Could not parse UI definition: %s", error->message);
		g_error_free (error);
	}
	data->dialog = dialog;

	ass = WID ("assistant");
	ctk_window_set_title (CTK_WINDOW (ass), _("Enable Fingerprint Login"));
	ctk_window_set_transient_for (CTK_WINDOW (ass), parent);
	ctk_window_set_position (CTK_WINDOW (ass), CTK_WIN_POS_CENTER_ON_PARENT);
	g_signal_connect (G_OBJECT (ass), "cancel",
			  G_CALLBACK (assistant_cancelled), data);
	g_signal_connect (G_OBJECT (ass), "close",
			  G_CALLBACK (assistant_cancelled), data);
	g_signal_connect (G_OBJECT (ass), "prepare",
			  G_CALLBACK (assistant_prepare), data);

	/* Page 1 */
	ctk_combo_box_set_active (CTK_COMBO_BOX (WID ("finger_combobox")), 0);

	g_signal_connect (G_OBJECT (WID ("radiobutton1")), "toggled",
			  G_CALLBACK (finger_radio_button_toggled), data);
	g_signal_connect (G_OBJECT (WID ("radiobutton2")), "toggled",
			  G_CALLBACK (finger_radio_button_toggled), data);
	g_signal_connect (G_OBJECT (WID ("radiobutton3")), "toggled",
			  G_CALLBACK (finger_radio_button_toggled), data);
	g_signal_connect (G_OBJECT (WID ("finger_combobox")), "changed",
			  G_CALLBACK (finger_combobox_changed), data);

	data->finger = selected_finger (dialog);

	g_object_set_data (G_OBJECT (WID("page1")), "name", "intro");

	/* translators:
	 * The variable is the name of the device, for example:
	 * "To enable fingerprint login, you need to save one of your fingerprints, using the
	 * 'Digital Persona U.are.U 4000/4000B' device." */
	msg = g_strdup_printf (_("To enable fingerprint login, you need to save one of your fingerprints, using the '%s' device."),
			       data->name);
	ctk_label_set_text (CTK_LABEL (WID("intro-label")), msg);
	g_free (msg);

	ctk_assistant_set_page_complete (CTK_ASSISTANT (ass), WID("page1"), TRUE);

	/* Page 2 */
	if (data->is_swipe != FALSE)
		ctk_assistant_set_page_title (CTK_ASSISTANT (ass), WID("page2"), _("Swipe finger on reader"));
	else
		ctk_assistant_set_page_title (CTK_ASSISTANT (ass), WID("page2"), _("Place finger on reader"));

	g_object_set_data (G_OBJECT (WID("page2")), "name", "enroll");

	msg = g_strdup_printf (TR(finger_str_to_msg (data->finger, data->is_swipe)), data->name);
	ctk_label_set_text (CTK_LABEL (WID("enroll-label")), msg);
	g_free (msg);

	/* Page 3 */
	g_object_set_data (G_OBJECT (WID("page3")), "name", "summary");

	data->ass = ass;
	ctk_widget_show_all (ass);
}

void
fingerprint_button_clicked (CtkBuilder *dialog,
			    CtkWidget *enable,
			    CtkWidget *disable)
{
	bindtextdomain ("fprintd", CAFELOCALEDIR);
	bind_textdomain_codeset ("fprintd", "UTF-8");

	if (is_disable != FALSE) {
		delete_fingerprints_question (dialog, enable, disable);
	} else {
		enroll_fingerprints (CTK_WINDOW (WID ("about-me-dialog")), enable, disable);
	}
}

