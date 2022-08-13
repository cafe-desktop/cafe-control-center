#include <config.h>
#include <ctk/ctk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include "dm-util.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define GSM_SERVICE_DBUS   "org.gnome.SessionManager"
#define GSM_PATH_DBUS      "/org/gnome/SessionManager"
#define GSM_INTERFACE_DBUS "org.gnome.SessionManager"

enum {
        GSM_LOGOUT_MODE_NORMAL = 0,
        GSM_LOGOUT_MODE_NO_CONFIRMATION,
        GSM_LOGOUT_MODE_FORCE
};

#include "capplet-util.h"
#include "activate-settings-daemon.h"

#define ACCESSIBILITY_KEY       "accessibility"
#define ACCESSIBILITY_SCHEMA    "org.cafe.interface"

static gboolean initial_state;

static GDesktopAppInfo *get_desktop_app_info_from_dm (void)
{
    DMType dm_type;
    GDesktopAppInfo *app_info = NULL;

    dm_type = dm_get_type();
    if (dm_type == DM_TYPE_MDM) {
        app_info = g_desktop_app_info_new ("mdmsetup.desktop");
    } else if (dm_type == DM_TYPE_LIGHTDM) {
        app_info = g_desktop_app_info_new ("lightdm-ctk-greeter-settings.desktop");
    }
    return app_info;
}

static GtkBuilder *
create_builder (void)
{
    GtkBuilder *builder;
    GError *error = NULL;

    builder = ctk_builder_new ();

    if (ctk_builder_add_from_resource (builder, "/org/cafe/mcc/accessibility/at/at-enable-dialog.ui", &error)) {
        GObject *object;
        GDesktopAppInfo *app_info = NULL;

        object = ctk_builder_get_object (builder, "at_enable_image");
        ctk_image_set_from_file (GTK_IMAGE (object),
                                 PIXMAPDIR "/at-startup.png");

        object = ctk_builder_get_object (builder,
                                         "at_applications_image");
        ctk_image_set_from_file (GTK_IMAGE (object),
                                 PIXMAPDIR "/at-support.png");
        app_info = get_desktop_app_info_from_dm ();
        if (app_info == NULL) {
            object = ctk_builder_get_object (builder, "login_button");
            ctk_widget_hide (GTK_WIDGET (object));
        } else {
            g_object_unref (app_info);
        }

    } else {
        g_warning ("Could not load UI: %s", error->message);
        g_error_free (error);
        g_object_unref (builder);
        builder = NULL;
    }

    return builder;
}

static void
cb_at_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("cafe-default-applications-properties --show-page=a11y", NULL);
}

static void
cb_keyboard_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("cafe-keyboard-properties --a11y", NULL);
}

static void
cb_mouse_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("cafe-mouse-properties --show-page=accessibility", NULL);
}

static void
cb_login_preferences (GtkDialog *dialog, gint response_id)
{
    GDesktopAppInfo *app_info = NULL;

    app_info = get_desktop_app_info_from_dm ();

    if (app_info != NULL) {
        g_app_info_launch (G_APP_INFO(app_info),  NULL, NULL, NULL);
        g_object_unref (app_info);
    }
}

static GDBusProxy *
get_sm_proxy (void)
{
    GError            *error = NULL;
    static GDBusProxy *proxy = NULL;

    if (proxy == NULL) {
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               GSM_SERVICE_DBUS,
                                               GSM_PATH_DBUS,
                                               GSM_INTERFACE_DBUS,
                                               NULL,
                                               &error);
        if (proxy == NULL) {
            g_warning ("Couldn't connect to session bus: %s", error->message);
            g_error_free (error);
        }
    }
    return proxy;
}

static gboolean
do_logout (void)
{
    GDBusProxy *sm_proxy;
    GError *error = NULL;
    GVariant *ret;
    gboolean res = FALSE;

    sm_proxy = get_sm_proxy ();
    if (sm_proxy == NULL)
        return FALSE;

    ret = g_dbus_proxy_call_sync (sm_proxy,
                                  "Logout",
                                  g_variant_new ("(u)", 0),    /* '0' means 'log out normally' */
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
    if (ret == NULL) {
        g_warning ("Could not call Logout by dbus: %s\n", error->message);
        g_error_free (error);
    } else {
        g_variant_unref (ret);
        res = TRUE;
    }

    if (sm_proxy)
        g_object_unref (sm_proxy);

    return res;
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "goscustaccess-11");
	else if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT)
		ctk_main_quit ();
	else {
	        g_message ("CLOSE AND LOGOUT!");

		if (!do_logout ())
                       ctk_main_quit ();
	}
}

static void
close_logout_update (GtkBuilder *builder)
{
	GSettings *settings = g_settings_new (ACCESSIBILITY_SCHEMA);
	gboolean curr_state = g_settings_get_boolean (settings, ACCESSIBILITY_KEY);
	GObject *btn = ctk_builder_get_object (builder,
					       "at_close_logout_button");

	ctk_widget_set_sensitive (GTK_WIDGET (btn), initial_state != curr_state);
	g_object_unref (settings);
}

static void
at_enable_toggled (GtkToggleButton *toggle_button,
		   GtkBuilder      *builder)
{
	GSettings *settings = g_settings_new (ACCESSIBILITY_SCHEMA);
	gboolean is_enabled = ctk_toggle_button_get_active (toggle_button);

	g_settings_set_boolean (settings, ACCESSIBILITY_KEY, is_enabled);
	g_object_unref (settings);
}

static void
at_enable_update (GSettings *settings,
		  GtkBuilder  *builder)
{
	gboolean is_enabled = g_settings_get_boolean (settings, ACCESSIBILITY_KEY);
	GObject *btn = ctk_builder_get_object (builder, "at_enable_toggle");

	ctk_toggle_button_set_active (GTK_TOGGLE_BUTTON (btn),
				      is_enabled);
}

static void
at_enable_changed (GSettings *settings,
		   gchar       *key,
		   gpointer     user_data)
{
	at_enable_update (settings, user_data);
	close_logout_update (user_data);
}

static void
setup_dialog (GtkBuilder *builder, GSettings *settings)
{
	GtkWidget *widget;
	GObject *object;

	object = ctk_builder_get_object (builder, "at_enable_toggle");
	g_signal_connect (object, "toggled",
			  G_CALLBACK (at_enable_toggled),
			  builder);

	initial_state = g_settings_get_boolean (settings, ACCESSIBILITY_KEY);

	at_enable_update (settings, builder);

	g_signal_connect (settings, "changed::" ACCESSIBILITY_KEY, G_CALLBACK (at_enable_changed),
				 builder);

	object = ctk_builder_get_object (builder, "at_pref_button");
	g_signal_connect (object, "clicked",
			  G_CALLBACK (cb_at_preferences), NULL);

	object = ctk_builder_get_object (builder, "keyboard_button");
	g_signal_connect (object, "clicked",
			  G_CALLBACK (cb_keyboard_preferences), NULL);

	object = ctk_builder_get_object (builder, "mouse_button");
	g_signal_connect (object, "clicked",
			  G_CALLBACK (cb_mouse_preferences), NULL);

	object = ctk_builder_get_object (builder, "login_button");
	g_signal_connect (object, "clicked",
			  G_CALLBACK (cb_login_preferences), NULL);

	widget = GTK_WIDGET (ctk_builder_get_object (builder,
						     "at_properties_dialog"));
	capplet_set_icon (widget, "preferences-desktop-accessibility");

	g_signal_connect (G_OBJECT (widget),
			  "response",
			  G_CALLBACK (cb_dialog_response), NULL);

	ctk_widget_show (widget);
}

int
main (int argc, char *argv[])
{
	GSettings  *settings;
	GtkBuilder *builder;

	capplet_init (NULL, &argc, &argv);

	activate_settings_daemon ();

	settings = g_settings_new (ACCESSIBILITY_SCHEMA);
	builder = create_builder ();

	if (builder) {

		setup_dialog (builder, settings);

		ctk_main ();

		g_object_unref (builder);
		g_object_unref (settings);
	}

	return 0;
}
