/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
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

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <string.h>

#include "theme-save.h"
#include "theme-util.h"
#include "capplet-util.h"

static GQuark error_quark;
enum {
  INVALID_THEME_NAME
};

/* taken from cafe-desktop-item.c */
static gchar *
escape_string_and_dup (const gchar *s)
{
  gchar *return_value, *p;
  const gchar *q;
  int len = 0;

  if (s == NULL)
    return g_strdup("");

  q = s;
  while (*q)
    {
      len++;
      if (strchr ("\n\r\t\\", *q) != NULL)
	len++;
      q++;
    }
  return_value = p = (gchar *) g_malloc (len + 1);
  do
    {
      switch (*s)
	{
	case '\t':
	  *p++ = '\\';
	  *p++ = 't';
	  break;
	case '\n':
	  *p++ = '\\';
	  *p++ = 'n';
	  break;
	case '\r':
	  *p++ = '\\';
	  *p++ = 'r';
	  break;
	case '\\':
	  *p++ = '\\';
	  *p++ = '\\';
	  break;
	default:
	  *p++ = *s;
	}
    }
  while (*s++);
  return return_value;
}

static gboolean
check_theme_name (const gchar  *theme_name,
		  GError      **error)
{
  if (theme_name == NULL) {
    g_set_error (error,
		 error_quark,
		 INVALID_THEME_NAME,
		 _("Theme name must be present"));
    return FALSE;
  }
  return TRUE;
}

static gchar *
str_remove_slash (const gchar *src)
{
  const gchar *i;
  gchar *rtn;
  gint len = 0;
  i = src;

  while (*i) {
    if (*i != '/')
      len++;
    i++;
  }

  rtn = (gchar *) g_malloc (len + 1);
  while (*src) {
    if (*src != '/') {
      *rtn = *src;
      rtn++;
    }
    src++;
  }
  *rtn = '\0';
  return rtn - len;
}

static gboolean
setup_directory_structure (const gchar  *theme_name,
			   GError      **error)
{
  gchar *dir, *theme_name_dir;
  gboolean retval = TRUE;

  theme_name_dir = str_remove_slash (theme_name);

  dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
  if (!g_file_test (dir, G_FILE_TEST_EXISTS))
    g_mkdir (dir, 0775);
  g_free (dir);

  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, NULL);
  if (!g_file_test (dir, G_FILE_TEST_EXISTS))
    g_mkdir (dir, 0775);
  g_free (dir);

  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, "index.theme", NULL);
  g_free (theme_name_dir);

  if (g_file_test (dir, G_FILE_TEST_EXISTS)) {
    CtkDialog *dialog;
    CtkWidget *button;
    gint response;

    dialog = (CtkDialog *) ctk_message_dialog_new (NULL,
						   CTK_DIALOG_MODAL,
						   CTK_MESSAGE_QUESTION,
	 					   CTK_BUTTONS_CANCEL,
						   _("The theme already exists. Would you like to replace it?"));
    button = ctk_dialog_add_button (dialog, _("_Overwrite"), CTK_RESPONSE_ACCEPT);
    ctk_button_set_image (CTK_BUTTON (button),
			  ctk_image_new_from_icon_name ("document-save", CTK_ICON_SIZE_BUTTON));
    response = ctk_dialog_run (dialog);
    ctk_widget_destroy (CTK_WIDGET (dialog));
    retval = (response != CTK_RESPONSE_CANCEL);
  }
  g_free (dir);

  return retval;
}

static gboolean
write_theme_to_disk (CafeThemeMetaInfo  *theme_info,
		     const gchar         *theme_name,
		     const gchar         *theme_description,
		     gboolean		  save_background,
		     gboolean		  save_notification,
		     GError             **error)
{
	gchar* dir;
	gchar* theme_name_dir;
	GFile* tmp_file;
	GFile* target_file;
	GOutputStream* output;

	gchar* str;
	gchar* current_background;

	GSettings* settings;
	const gchar* theme_header = ""
		"[Desktop Entry]\n"
		"Name=%s\n"
		"Type=X-GNOME-Metatheme\n"
		"Comment=%s\n"
		"\n"
		"[X-GNOME-Metatheme]\n"
		"CtkTheme=%s\n"
		"MetacityTheme=%s\n"
		"IconTheme=%s\n";

  theme_name_dir = str_remove_slash (theme_name);
  dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, "index.theme~", NULL);
  g_free (theme_name_dir);

  tmp_file = g_file_new_for_path (dir);
  dir [strlen (dir) - 1] = '\000';
  target_file = g_file_new_for_path (dir);
  g_free (dir);

  /* start making the theme file */
  str = g_strdup_printf(theme_header, theme_name, theme_description, theme_info->ctk_theme_name, theme_info->croma_theme_name, theme_info->icon_theme_name);

  output = G_OUTPUT_STREAM (g_file_replace (tmp_file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL));
  g_output_stream_write (output, str, strlen (str), NULL, NULL);
  g_free (str);

  if (theme_info->ctk_color_scheme) {
    gchar *a, *tmp;
    tmp = g_strdup (theme_info->ctk_color_scheme);
    for (a = tmp; *a != '\0'; a++)
      if (*a == '\n')
        *a = ',';
    str = g_strdup_printf ("CtkColorScheme=%s\n", tmp);
    g_output_stream_write (output, str, strlen (str), NULL, NULL);

    g_free (str);
    g_free (tmp);
  }

  if (theme_info->cursor_theme_name) {
    str = g_strdup_printf ("CursorTheme=%s\n"
                           "CursorSize=%i\n",
                           theme_info->cursor_theme_name,
                           theme_info->cursor_size);
    g_output_stream_write (output, str, strlen (str), NULL, NULL);
    g_free (str);
  }

  if (theme_info->notification_theme_name && save_notification) {
    str = g_strdup_printf ("NotificationTheme=%s\n", theme_info->notification_theme_name);
    g_output_stream_write (output, str, strlen (str), NULL, NULL);
    g_free (str);
  }

  if (save_background) {
    settings = g_settings_new (WP_SCHEMA);
    current_background = g_settings_get_string (settings, WP_FILE_KEY);

    if (current_background != NULL) {
      str = g_strdup_printf ("BackgroundImage=%s\n", current_background);

      g_output_stream_write (output, str, strlen (str), NULL, NULL);

      g_free (current_background);
      g_free (str);
    }
    g_object_unref (settings);
  }

  g_file_move (tmp_file, target_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
  g_output_stream_close (output, NULL, NULL);

  g_object_unref (tmp_file);
  g_object_unref (target_file);

  return TRUE;
}

static gboolean
save_theme_to_disk (CafeThemeMetaInfo  *theme_info,
		    const gchar         *theme_name,
		    const gchar         *theme_description,
		    gboolean		 save_background,
		    gboolean             save_notification,
		    GError             **error)
{
  if (!check_theme_name (theme_name, error))
    return FALSE;

  if (!setup_directory_structure (theme_name, error))
    return FALSE;

  if (!write_theme_to_disk (theme_info, theme_name, theme_description, save_background, save_notification, error))
    return FALSE;

  return TRUE;
}

static void
save_dialog_response (CtkWidget      *save_dialog,
		      gint            response_id,
		      AppearanceData *data)
{
  if (response_id == CTK_RESPONSE_OK) {
    CtkWidget *entry;
    CtkWidget *text_view;
    CtkTextBuffer *buffer;
    CtkTextIter start_iter;
    CtkTextIter end_iter;
    gchar *buffer_text;
    CafeThemeMetaInfo *theme_info;
    gchar *theme_description = NULL;
    gchar *theme_name = NULL;
    gboolean save_background;
    gboolean save_notification;
    GError *error = NULL;

    entry = appearance_capplet_get_widget (data, "save_dialog_entry");
    theme_name = escape_string_and_dup (ctk_entry_get_text (CTK_ENTRY (entry)));

    text_view = appearance_capplet_get_widget (data, "save_dialog_textview");
    buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (text_view));
    ctk_text_buffer_get_start_iter (buffer, &start_iter);
    ctk_text_buffer_get_end_iter (buffer, &end_iter);
    buffer_text = ctk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
    theme_description = escape_string_and_dup (buffer_text);
    g_free (buffer_text);
    theme_info = (CafeThemeMetaInfo *) g_object_get_data (G_OBJECT (save_dialog), "meta-theme-info");
    save_background = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (
		      appearance_capplet_get_widget (data, "save_background_checkbutton")));
    save_notification = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (
			appearance_capplet_get_widget (data, "save_notification_checkbutton")));

    if (save_theme_to_disk (theme_info, theme_name, theme_description, save_background, save_notification, &error)) {
      /* remove the custom theme */
      CtkTreeIter iter;

      if (theme_find_in_model (CTK_TREE_MODEL (data->theme_store), "__custom__", &iter))
        ctk_list_store_remove (data->theme_store, &iter);
    }

    g_free (theme_name);
    g_free (theme_description);
    g_clear_error (&error);
  }

  ctk_widget_hide (save_dialog);
}

static void
entry_text_changed (CtkEditable *editable,
                    AppearanceData  *data)
{
  const gchar *text;
  CtkWidget *button;

  text = ctk_entry_get_text (CTK_ENTRY (editable));
  button = appearance_capplet_get_widget (data, "save_dialog_save_button");

  ctk_widget_set_sensitive (button, text != NULL && text[0] != '\000');
}

void
theme_save_dialog_run (CafeThemeMetaInfo *theme_info,
		       AppearanceData     *data)
{
  CtkWidget *entry;
  CtkWidget *text_view;
  CtkTextBuffer *text_buffer;

  entry = appearance_capplet_get_widget (data, "save_dialog_entry");
  text_view = appearance_capplet_get_widget (data, "save_dialog_textview");

  if (data->theme_save_dialog == NULL) {
    data->theme_save_dialog = appearance_capplet_get_widget (data, "theme_save_dialog");

    g_signal_connect (data->theme_save_dialog, "response", (GCallback) save_dialog_response, data);
    g_signal_connect (data->theme_save_dialog, "delete-event", (GCallback) ctk_true, NULL);
    g_signal_connect (entry, "changed", (GCallback) entry_text_changed, data);

    error_quark = g_quark_from_string ("cafe-theme-save");
    ctk_widget_set_size_request (text_view, 300, 100);
  }

  ctk_entry_set_text (CTK_ENTRY (entry), "");
  entry_text_changed (CTK_EDITABLE (entry), data);
  ctk_widget_grab_focus (entry);

  text_buffer = ctk_text_view_get_buffer (CTK_TEXT_VIEW (text_view));
  ctk_text_buffer_set_text (text_buffer, "", 0);
  g_object_set_data (G_OBJECT (data->theme_save_dialog), "meta-theme-info", theme_info);
  ctk_window_set_transient_for (CTK_WINDOW (data->theme_save_dialog),
                                CTK_WINDOW (appearance_capplet_get_widget (data, "appearance_window")));
  ctk_widget_show (data->theme_save_dialog);
}
