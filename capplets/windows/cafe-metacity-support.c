/* cafe-metacity-support.c
 * Copyright (C) 2014 Stefano Karapetsas
 *
 * Written by: Stefano Karapetsas <stefano@karapetsas.com>
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
#include <gio/gio.h>
#include <ctk/ctk.h>

#define METACITY_SCHEMA "org.gnome.metacity"
#define COMPOSITING_KEY "compositing-manager"

void
cafe_metacity_config_tool ()
{
    GSettings *settings;
    CtkDialog *dialog;
    CtkWidget *vbox;
    CtkWidget *widget;
    gchar *str;

    settings = g_settings_new (METACITY_SCHEMA);

    dialog = CTK_DIALOG (ctk_dialog_new_with_buttons(_("Metacity Preferences"),
                                                     NULL,
                                                     CTK_DIALOG_MODAL,
                                                     "ctk-close",
                                                     CTK_RESPONSE_CLOSE,
                                                     NULL));
    ctk_window_set_icon_name (CTK_WINDOW (dialog), "preferences-system-windows");
    ctk_window_set_default_size (CTK_WINDOW (dialog), 350, 150);

    vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);

    str = g_strdup_printf ("<b>%s</b>", _("Compositing Manager"));
    widget = ctk_label_new (str);
    g_free (str);
    ctk_label_set_use_markup (CTK_LABEL (widget), TRUE);
    ctk_label_set_xalign (CTK_LABEL (widget), 0.0);
    ctk_box_pack_start (CTK_BOX (vbox), widget, FALSE, FALSE, 0);

    widget = ctk_check_button_new_with_label (_("Enable software _compositing window manager"));
    ctk_box_pack_start (CTK_BOX (vbox), widget, FALSE, FALSE, 0);
    g_settings_bind (settings, COMPOSITING_KEY, widget, "active", G_SETTINGS_BIND_DEFAULT);

    ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (dialog)), vbox, TRUE, TRUE, 0);

    g_signal_connect (dialog, "response", G_CALLBACK (ctk_main_quit), dialog);
    ctk_widget_show_all (CTK_WIDGET (dialog));
    ctk_main ();

    g_object_unref (settings);
}
