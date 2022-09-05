/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
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

#include <glib/gi18n.h>
#include <libcafe-desktop/cafe-gsettings.h>
#include "appearance.h"
#include "appearance-desktop.h"
#include "appearance-font.h"
#include "appearance-themes.h"
#include "appearance-style.h"
#include "appearance-ui.h"
#include "appearance-support.h"
#include "theme-installer.h"
#include "theme-thumbnail.h"
#include "activate-settings-daemon.h"
#include "capplet-util.h"

static AppearanceData *
init_appearance_data (int *argc, char ***argv, GOptionContext *context)
{
  AppearanceData *data = NULL;
  CtkBuilder *ui;
  GError *err = NULL;

  theme_thumbnail_factory_init (*argc, *argv);
  capplet_init (context, argc, argv);
  activate_settings_daemon ();

  /* set up the data */
  ui = ctk_builder_new ();
  ctk_builder_add_from_resource (ui, "/org/cafe/mcc/appearance/data/appearance.ui", &err);

  if (err)
    {
      g_warning (_("Could not load user interface file: %s"), err->message);
      g_error_free (err);
      g_object_unref (ui);
    }
  else
    {
      data = g_new (AppearanceData, 1);
      data->settings = g_settings_new (APPEARANCE_SCHEMA);
      data->wp_settings = g_settings_new (WP_SCHEMA);

      if (cafe_gsettings_schema_exists (CAJA_SCHEMA))
        data->baul_settings = g_settings_new (CAJA_SCHEMA);
      else
        data->baul_settings = NULL;

      data->interface_settings = g_settings_new (INTERFACE_SCHEMA);
      data->croma_settings = g_settings_new (CROMA_SCHEMA);
      data->mouse_settings = g_settings_new (MOUSE_SCHEMA);
      data->font_settings = g_settings_new (FONT_RENDER_SCHEMA);
      data->ui = ui;
      data->thumb_factory = cafe_desktop_thumbnail_factory_new (CAFE_DESKTOP_THUMBNAIL_SIZE_NORMAL);
    }

  return data;
}

static void
main_window_response (CtkWidget *widget,
                      gint response_id,
                      AppearanceData *data)
{
  if (response_id == CTK_RESPONSE_CLOSE ||
      response_id == CTK_RESPONSE_DELETE_EVENT)
  {
    ctk_main_quit ();

    themes_shutdown (data);
    style_shutdown (data);
    desktop_shutdown (data);
    font_shutdown (data);
    support_shutdown (data);

    g_object_unref (data->thumb_factory);
    g_object_unref (data->settings);
    g_object_unref (data->wp_settings);

    if (data->baul_settings)
      g_object_unref (data->baul_settings);

    g_object_unref (data->interface_settings);
    g_object_unref (data->croma_settings);
    g_object_unref (data->mouse_settings);
    g_object_unref (data->font_settings);
    g_object_unref (data->ui);
  }
  else if (response_id == CTK_RESPONSE_HELP)
  {
      CtkNotebook *nb;
      gint pindex;

      nb = CTK_NOTEBOOK (appearance_capplet_get_widget (data, "main_notebook"));
      pindex = ctk_notebook_get_current_page (nb);

      switch (pindex)
      {
        case 0: /* theme */
          capplet_help (CTK_WINDOW (widget), "goscustdesk-12");
          break;
        case 1: /* background */
          capplet_help (CTK_WINDOW (widget), "goscustdesk-7");
          break;
        case 2: /* fonts */
          capplet_help (CTK_WINDOW (widget), "goscustdesk-38");
          break;
        case 3: /* interface */
          capplet_help (CTK_WINDOW (widget), "goscustuserinter-2");
          break;
        default:
          capplet_help (CTK_WINDOW (widget), "prefs-look-and-feel");
          break;
       }
  }
}

int
main (int argc, char **argv)
{
  AppearanceData *data;
  CtkWidget *w;
  CtkStyleContext *context;

  gchar *install_filename = NULL;
  gchar *start_page = NULL;
  gchar **wallpaper_files = NULL;
  GOptionContext *option_context;
  GOptionEntry option_entries[] = {
      { "install-theme",
        'i',
        G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_FILENAME,
        &install_filename,
        N_("Specify the filename of a theme to install"),
        N_("filename") },
      { "show-page",
        'p',
        G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_STRING,
        &start_page,
        /* TRANSLATORS: don't translate the terms in brackets */
        N_("Specify the name of the page to show (theme|background|fonts|interface)"),
        N_("page") },
      { G_OPTION_REMAINING,
      	0,
      	G_OPTION_FLAG_IN_MAIN,
      	G_OPTION_ARG_FILENAME_ARRAY,
      	&wallpaper_files,
      	NULL,
      	N_("[WALLPAPER...]") },
      { NULL }
    };

  option_context = g_option_context_new (NULL);
  g_option_context_add_main_entries (option_context, option_entries, GETTEXT_PACKAGE);

  /* init */
  data = init_appearance_data (&argc, &argv, option_context);
  if (!data)
    return 1;

  /* init tabs */
  themes_init (data);
  style_init (data);
  desktop_init (data, (const gchar **) wallpaper_files);
  g_strfreev (wallpaper_files);
  font_init (data);
  ui_init (data);

  /* init support for other window managers */
  support_init (data);

  /* prepare the main window */
  w = appearance_capplet_get_widget (data, "appearance_window");
  context = ctk_widget_get_style_context (CTK_WIDGET (w));
  ctk_style_context_add_class (context, "appearance-window");

  capplet_set_icon (w, "preferences-desktop-theme");
  ctk_widget_show_all (w);

  g_signal_connect_after (w, "response",
                          (GCallback) main_window_response, data);

  /* default to background page if files were given on the command line */
  if (wallpaper_files && !install_filename && !start_page)
    start_page = g_strdup ("background");

  CtkNotebook* nb = CTK_NOTEBOOK(appearance_capplet_get_widget (data, "main_notebook"));
  ctk_widget_add_events (CTK_WIDGET (nb), CDK_SCROLL_MASK);
  g_signal_connect (CTK_WIDGET (nb), "scroll-event",
                    G_CALLBACK (capplet_dialog_page_scroll_event_cb),
                    CTK_WINDOW (w));

  if (start_page != NULL) {
    gchar *page_name;

    page_name = g_strconcat (start_page, "_vbox", NULL);
    g_free (start_page);

    w = appearance_capplet_get_widget (data, page_name);
    if (w != NULL) {
      gint pindex;

      pindex = ctk_notebook_page_num (nb, w);
      if (pindex != -1)
        ctk_notebook_set_current_page (nb, pindex);
    }
    g_free (page_name);
  }

  if (install_filename != NULL) {
    GFile *inst = g_file_new_for_commandline_arg (install_filename);
    g_free (install_filename);
    cafe_theme_install (inst, CTK_WINDOW (w));
    g_object_unref (inst);
  }

  g_option_context_free (option_context);

  /* start the mainloop */
  ctk_main ();

  /* free stuff */
  g_free (data);

  return 0;
}
