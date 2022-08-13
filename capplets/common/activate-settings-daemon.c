#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cafe-settings-client.h>
#include <ctk/ctk.h>
#include <glib/gi18n.h>

#include "activate-settings-daemon.h"

static void popup_error_message (void)
{
  CtkWidget *dialog;

  dialog = ctk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING,
				   GTK_BUTTONS_OK, _("Unable to start the settings manager 'cafe-settings-daemon'.\n"
				   "Without the CAFE settings manager running, some preferences may not take effect. This could "
				   "indicate a problem with DBus, or a non-CAFE (e.g. KDE) settings manager may already "
				   "be active and conflicting with the CAFE settings manager."));

  ctk_dialog_run (GTK_DIALOG (dialog));
  ctk_widget_destroy (dialog);
}

/* Returns FALSE if activation failed, else TRUE */
gboolean
activate_settings_daemon (void)
{
  DBusGConnection *connection = NULL;
  DBusGProxy *proxy = NULL;
  GError *error = NULL;

  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (connection == NULL)
    {
      popup_error_message ();
      g_error_free (error);
      return FALSE;
    }

  proxy = dbus_g_proxy_new_for_name (connection,
                                     "org.cafe.SettingsDaemon",
                                     "/org/cafe/SettingsDaemon",
                                     "org.cafe.SettingsDaemon");

  if (proxy == NULL)
    {
      popup_error_message ();
      return FALSE;
    }

  if (!org_cafe_SettingsDaemon_awake(proxy, &error))
    {
      popup_error_message ();
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}
