/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "capplet-util.h"
#include "file-transfer-dialog.h"
#include "theme-installer.h"
#include "theme-util.h"

enum {
	THEME_INVALID,
	THEME_ICON,
	THEME_CAFE,
	THEME_CTK,
	THEME_ENGINE,
	THEME_CROMA,
	THEME_CURSOR,
	THEME_ICON_CURSOR
};

enum {
	TARGZ,
	TARBZ,
	TARXZ,
	DIRECTORY
};

static gboolean
cleanup_tmp_dir (GIOSchedulerJob *job,
		 GCancellable *cancellable,
		 const gchar *tmp_dir)
{
	GFile *directory;

	directory = g_file_new_for_path (tmp_dir);
	capplet_file_delete_recursive (directory, NULL);
	g_object_unref (directory);

	return FALSE;
}

static int
file_theme_type (const gchar *dir)
{
	gchar *filename = NULL;
	gboolean exists;

	if (!dir)
		return THEME_INVALID;

	filename = g_build_filename (dir, "index.theme", NULL);
	exists = g_file_test (filename, G_FILE_TEST_IS_REGULAR);

	if (exists) {
		GPatternSpec *pattern;
		gchar *file_contents = NULL;
		gsize file_size;
		gboolean match;

		g_file_get_contents (filename, &file_contents, &file_size, NULL);
		g_free (filename);

		pattern = g_pattern_spec_new ("*[Icon Theme]*");
		match = g_pattern_match_string (pattern, file_contents);
		g_pattern_spec_free (pattern);

		if (match) {
			pattern = g_pattern_spec_new ("*Directories=*");
			match = g_pattern_match_string (pattern, file_contents);
			g_pattern_spec_free (pattern);
			g_free (file_contents);

			if (match) {
				/* check if we have a cursor, too */
				filename = g_build_filename (dir, "cursors", NULL);
				exists = g_file_test (filename, G_FILE_TEST_IS_DIR);
				g_free (filename);

				if (exists)
					return THEME_ICON_CURSOR;
				else
					return THEME_ICON;
			}
			return THEME_CURSOR;
		}

		pattern = g_pattern_spec_new ("*[X-GNOME-Metatheme]*");
		match = g_pattern_match_string (pattern, file_contents);
		g_pattern_spec_free (pattern);
		g_free (file_contents);

		if (match)
			return THEME_CAFE;
	} else {
		g_free (filename);
	}

	filename = g_build_filename (dir, "ctk-2.0", "ctkrc", NULL);
	exists = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
	g_free (filename);

	if (exists)
		return THEME_CTK;

	filename = g_build_filename (dir, "metacity-1", "metacity-theme-2.xml", NULL);
	exists = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
	g_free (filename);

	if (exists)
		return THEME_CROMA;

	filename = g_build_filename (dir, "metacity-1", "metacity-theme-1.xml", NULL);
	exists = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
	g_free (filename);

	if (exists)
		return THEME_CROMA;

	/* cursor themes don't necessarily have an index.theme */
	filename = g_build_filename (dir, "cursors", NULL);
	exists = g_file_test (filename, G_FILE_TEST_IS_DIR);
	g_free (filename);

	if (exists)
		return THEME_CURSOR;

	filename = g_build_filename (dir, "configure", NULL);
	exists = g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE);
	g_free (filename);

	if (exists)
		return THEME_ENGINE;

	return THEME_INVALID;
}

static void
transfer_cancel_cb (CtkWidget *dialog,
		    gchar *path)
{
	GFile *todelete;

	todelete = g_file_new_for_path (path);
	capplet_file_delete_recursive (todelete, NULL);

	g_object_unref (todelete);
	g_free (path);
	ctk_widget_destroy (dialog);
}

static void
missing_utility_message_dialog (CtkWindow *parent,
				const gchar *utility)
{
	CtkWidget *dialog = ctk_message_dialog_new (parent,
						    CTK_DIALOG_MODAL,
						    CTK_MESSAGE_ERROR,
						    CTK_BUTTONS_OK,
						    _("Cannot install theme"));
	ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
						  _("The %s utility is not installed."), utility);
	ctk_dialog_run (CTK_DIALOG (dialog));
	ctk_widget_destroy (dialog);
}

/* this works around problems when doing fork/exec in a threaded app
 * with some locks being held/waited on in different threads.
 *
 * we do the idle callback so that the async xfer has finished and
 * cleaned up its vfs job.  otherwise it seems the slave thread gets
 * woken up and it removes itself from the job queue before it is
 * supposed to.  very strange.
 *
 * see bugzilla.gnome.org #86141 for details
 */
static gboolean
process_local_theme_tgz_tbz (CtkWindow *parent,
			     const gchar *util,
			     const gchar *tmp_dir,
			     const gchar *archive)
{
	gboolean rc;
	int status;
	gchar *command, *filename, *zip, *tar;

	if (!(zip = g_find_program_in_path (util))) {
		missing_utility_message_dialog (parent, util);
		return FALSE;
	}
	if (!(tar = g_find_program_in_path ("tar"))) {
		missing_utility_message_dialog (parent, "tar");
		g_free (zip);
		return FALSE;
	}

	filename = g_shell_quote (archive);

	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'cd \"%s\"; %s -d -c < \"%s\" | %s xf - '",
				   tmp_dir, zip, filename, tar);
	g_free (zip);
	g_free (tar);
	g_free (filename);

	rc = (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0);
	g_free (command);

	if (rc == FALSE) {
		CtkWidget *dialog;

		dialog = ctk_message_dialog_new (parent,
						 CTK_DIALOG_MODAL,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_OK,
						 _("Cannot install theme"));
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
							  _("There was a problem while extracting the theme."));
		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
	}

	return rc;
}

static gboolean
process_local_theme_archive (CtkWindow *parent,
			     gint filetype,
			     const gchar *tmp_dir,
			     const gchar *archive)
{
	if (filetype == TARGZ)
		return process_local_theme_tgz_tbz (parent, "gzip", tmp_dir, archive);
	else if (filetype == TARBZ)
		return process_local_theme_tgz_tbz (parent, "bzip2", tmp_dir, archive);
	else if (filetype == TARXZ)
		return process_local_theme_tgz_tbz (parent, "xz", tmp_dir, archive);
	else
		return FALSE;
}

static void
invalid_theme_dialog (CtkWindow *parent,
		      const gchar *filename,
		      gboolean maybe_theme_engine)
{
	CtkWidget *dialog;
	const gchar *primary = _("There was an error installing the selected file");
	const gchar *secondary = _("\"%s\" does not appear to be a valid theme.");
	const gchar *engine = _("\"%s\" does not appear to be a valid theme. It may be a theme engine which you need to compile.");

	dialog = ctk_message_dialog_new (parent,
					 CTK_DIALOG_MODAL,
					 CTK_MESSAGE_ERROR,
					 CTK_BUTTONS_OK,
					 "%s", primary);
	if (maybe_theme_engine)
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog), engine, filename);
	else
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog), secondary, filename);
	ctk_dialog_run (CTK_DIALOG (dialog));
	ctk_widget_destroy (dialog);
}

static gboolean
cafe_theme_install_real (CtkWindow *parent,
			  const gchar *tmp_dir,
			  const gchar *theme_name,
			  gboolean ask_user)
{
	gboolean success = TRUE;
	CtkWidget *dialog, *apply_button;
	GFile *theme_source_dir, *theme_dest_dir;
	GError *error = NULL;
	gint theme_type;
	gchar *target_dir = NULL;

	/* What type of theme is it? */
	theme_type = file_theme_type (tmp_dir);
	switch (theme_type) {
	case THEME_ICON:
	case THEME_CURSOR:
	case THEME_ICON_CURSOR:
		target_dir = g_build_path (G_DIR_SEPARATOR_S,
					   g_get_home_dir (), ".icons",
					   theme_name, NULL);
		break;
	case THEME_CAFE:
	case THEME_CROMA:
	case THEME_CTK:
		target_dir = g_build_path (G_DIR_SEPARATOR_S,
					   g_get_home_dir (), ".themes",
					   theme_name, NULL);
		break;
	case THEME_ENGINE:
		invalid_theme_dialog (parent, theme_name, TRUE);
		return FALSE;
	default:
		invalid_theme_dialog (parent, theme_name, FALSE);
		return FALSE;
	}

	/* see if there is an icon theme lurking in this package */
	if (theme_type == THEME_CAFE) {
		gchar *path;

		path = g_build_path (G_DIR_SEPARATOR_S,
				     tmp_dir, "icons", NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)
		    && (file_theme_type (path) == THEME_ICON)) {
			gchar *new_path, *update_icon_cache;
			GFile *new_file;
			GFile *src_file;

			src_file = g_file_new_for_path (path);
			new_path = g_build_path (G_DIR_SEPARATOR_S,
						 g_get_home_dir (),
						 ".icons",
						 theme_name, NULL);
			new_file = g_file_new_for_path (new_path);

			if (!g_file_move (src_file, new_file, G_FILE_COPY_NONE,
					  NULL, NULL, NULL, &error)) {
				g_warning ("Error while moving from `%s' to `%s': %s",
					   path, new_path, error->message);
				g_error_free (error);
				error = NULL;
			}
			g_object_unref (new_file);
			g_object_unref (src_file);

			/* update icon cache - shouldn't really matter if this fails */
			update_icon_cache = g_strdup_printf ("ctk-update-icon-cache %s", new_path);
			g_spawn_command_line_async (update_icon_cache, NULL);
			g_free (update_icon_cache);

			g_free (new_path);
		}
		g_free (path);
	}

	/* Move the dir to the target dir */
	theme_source_dir = g_file_new_for_path (tmp_dir);
	theme_dest_dir = g_file_new_for_path (target_dir);

	if (g_file_test (target_dir, G_FILE_TEST_EXISTS)) {
		gchar *str;

		str = g_strdup_printf (_("The theme \"%s\" already exists."), theme_name);
		dialog = ctk_message_dialog_new (parent,
						 CTK_DIALOG_MODAL,
						 CTK_MESSAGE_INFO,
						 CTK_BUTTONS_NONE,
						 "%s",
						 str);
		g_free (str);

		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
							  _("Do you want to install it again?"));

		ctk_dialog_add_button (CTK_DIALOG (dialog),
					   _("Cancel"),
					   CTK_RESPONSE_CLOSE);

		apply_button = ctk_button_new_with_label (_("Install"));
		ctk_button_set_image (CTK_BUTTON (apply_button),
					  ctk_image_new_from_icon_name ("ctk-apply",
								CTK_ICON_SIZE_BUTTON));
		ctk_dialog_add_action_widget (CTK_DIALOG (dialog), apply_button, CTK_RESPONSE_APPLY);
		ctk_widget_set_can_default (apply_button, TRUE);
		ctk_widget_show (apply_button);

		ctk_dialog_set_default_response (CTK_DIALOG (dialog), CTK_RESPONSE_APPLY);

		if (ctk_dialog_run (CTK_DIALOG (dialog)) == CTK_RESPONSE_APPLY) {
			ctk_widget_destroy (dialog);

			if (!capplet_file_delete_recursive (theme_dest_dir, NULL)) {
				CtkWidget *info_dialog = ctk_message_dialog_new (NULL,
									   CTK_DIALOG_MODAL,
									   CTK_MESSAGE_ERROR,
									   CTK_BUTTONS_OK,
									   _("Theme cannot be deleted"));
				ctk_dialog_run (CTK_DIALOG (info_dialog));
				ctk_widget_destroy (info_dialog);
				success = FALSE;
				goto end;
			}
		}else{
			ctk_widget_destroy (dialog);
			success = FALSE;
			goto end;
		}
	}

	if (!g_file_move (theme_source_dir, theme_dest_dir,
			  G_FILE_COPY_OVERWRITE, NULL, NULL,
			  NULL, &error)) {
		gchar *str;

		str = g_strdup_printf (_("Installation for theme \"%s\" failed."), theme_name);
		dialog = ctk_message_dialog_new (parent,
						 CTK_DIALOG_MODAL,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_OK,
						 "%s",
						 str);
		ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);

		g_free (str);
		g_error_free (error);
		error = NULL;

		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		success = FALSE;
	} else {
		if (theme_type == THEME_ICON || theme_type == THEME_ICON_CURSOR)
		{
			gchar *update_icon_cache;

			/* update icon cache - shouldn't really matter if this fails */
			update_icon_cache = g_strdup_printf ("ctk-update-icon-cache %s", target_dir);
			g_spawn_command_line_async (update_icon_cache, NULL);

			g_free (update_icon_cache);
		}

		if (ask_user) {
			/* Ask to apply theme (if we can) */
			if (theme_type == THEME_CTK
			    || theme_type == THEME_CROMA
			    || theme_type == THEME_ICON
			    || theme_type == THEME_CURSOR
			    || theme_type == THEME_ICON_CURSOR) {
				/* TODO: currently cannot apply "cafe themes" */
				gchar *str;

				str = g_strdup_printf (_("The theme \"%s\" has been installed."), theme_name);
				dialog = ctk_message_dialog_new_with_markup (parent,
									     CTK_DIALOG_MODAL,
									     CTK_MESSAGE_INFO,
									     CTK_BUTTONS_NONE,
									     "<span weight=\"bold\" size=\"larger\">%s</span>",
									     str);
				g_free (str);

				ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
									  _("Would you like to apply it now, or keep your current theme?"));

				ctk_dialog_add_button (CTK_DIALOG (dialog),
						       _("Keep Current Theme"),
						       CTK_RESPONSE_CLOSE);

				apply_button = ctk_button_new_with_label (_("Apply New Theme"));
				ctk_button_set_image (CTK_BUTTON (apply_button),
						      ctk_image_new_from_icon_name ("ctk-apply",
										    CTK_ICON_SIZE_BUTTON));
				ctk_dialog_add_action_widget (CTK_DIALOG (dialog), apply_button, CTK_RESPONSE_APPLY);
				ctk_widget_set_can_default (apply_button, TRUE);
				ctk_widget_show (apply_button);

				ctk_dialog_set_default_response (CTK_DIALOG (dialog), CTK_RESPONSE_APPLY);

				if (ctk_dialog_run (CTK_DIALOG (dialog)) == CTK_RESPONSE_APPLY) {
					/* apply theme here! */
					GSettings *settings;

					switch (theme_type) {
					case THEME_CTK:
						settings = g_settings_new (INTERFACE_SCHEMA);
						g_settings_set_string (settings, CTK_THEME_KEY, theme_name);
						g_object_unref (settings);
						break;
					case THEME_CROMA:
						settings = g_settings_new (CROMA_SCHEMA);
						g_settings_set_string (settings, CROMA_THEME_KEY, theme_name);
						g_object_unref (settings);
						break;
					case THEME_ICON:
						settings = g_settings_new (INTERFACE_SCHEMA);
						g_settings_set_string (settings, ICON_THEME_KEY, theme_name);
						g_object_unref (settings);
						break;
					case THEME_CURSOR:
						settings = g_settings_new (MOUSE_SCHEMA);
						g_settings_set_string (settings, CURSOR_THEME_KEY, theme_name);
						g_object_unref (settings);
						break;
					case THEME_ICON_CURSOR:
						settings = g_settings_new (INTERFACE_SCHEMA);
						g_settings_set_string (settings, ICON_THEME_KEY, theme_name);
						g_object_unref (settings);
						settings = g_settings_new (MOUSE_SCHEMA);
						g_settings_set_string (settings, CURSOR_THEME_KEY, theme_name);
						g_object_unref (settings);
						break;
					default:
						break;
					}
				}
			} else {
				dialog = ctk_message_dialog_new (parent,
								 CTK_DIALOG_MODAL,
								 CTK_MESSAGE_INFO,
								 CTK_BUTTONS_OK,
								 _("CAFE Theme %s correctly installed"),
								 theme_name);
				ctk_dialog_run (CTK_DIALOG (dialog));
			}
			ctk_widget_destroy (dialog);
		}
	}

end:
	g_free (target_dir);
	g_object_unref (theme_source_dir);
	g_object_unref (theme_dest_dir);

	return success;
}

static void
process_local_theme (CtkWindow  *parent,
		     const char *path)
{
	CtkWidget *dialog;
	gint filetype;

	if (g_str_has_suffix (path, ".tar.gz")
	    || g_str_has_suffix (path, ".tgz")
	    || g_str_has_suffix(path, ".gtp")) {
		filetype = TARGZ;
	} else if (g_str_has_suffix (path, ".tar.bz2")) {
		filetype = TARBZ;
	} else if (g_str_has_suffix (path, ".tar.xz")) {
		filetype = TARXZ;
	} else if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		filetype = DIRECTORY;
	} else {
		gchar *filename;
		filename = g_path_get_basename (path);
		invalid_theme_dialog (parent, filename, FALSE);
		g_free (filename);
		return;
	}

	if (filetype == DIRECTORY) {
		gchar *name = g_path_get_basename (path);
		cafe_theme_install_real (parent,
					  path,
					  name,
					  TRUE);
		g_free (name);
	} else {
		/* Create a temp directory and uncompress file there */
		GDir *dir;
		const gchar *name;
		gchar *tmp_dir;
		gboolean ok;
		gint n_themes;
		GFile *todelete;

		tmp_dir = g_strdup_printf ("%s/.themes/.theme-%u",
					   g_get_home_dir (),
					   g_random_int ());

		if ((g_mkdir (tmp_dir, 0700)) != 0) {
			dialog = ctk_message_dialog_new (parent,
							 CTK_DIALOG_MODAL,
							 CTK_MESSAGE_ERROR,
							 CTK_BUTTONS_OK,
							 _("Failed to create temporary directory"));
			ctk_dialog_run (CTK_DIALOG (dialog));
			ctk_widget_destroy (dialog);
			g_free (tmp_dir);
			return;
		}

		if (!process_local_theme_archive (parent, filetype, tmp_dir, path)
		    || ((dir = g_dir_open (tmp_dir, 0, NULL)) == NULL)) {
			g_io_scheduler_push_job ((GIOSchedulerJobFunc) cleanup_tmp_dir,
						 g_strdup (tmp_dir),
						 g_free,
						 G_PRIORITY_DEFAULT,
						 NULL);
			g_free (tmp_dir);
			return;
		}

		todelete = g_file_new_for_path (path);
		g_file_delete (todelete, NULL, NULL);
		g_object_unref (todelete);

		/* See whether we have multiple themes to install. If so,
		 * we won't ask the user whether to apply the new theme
		 * after installation. */
		n_themes = 0;
		for (name = g_dir_read_name (dir);
		     name && n_themes <= 1;
		     name = g_dir_read_name (dir)) {
			gchar *theme_dir;

			theme_dir = g_build_filename (tmp_dir, name, NULL);

			if (g_file_test (theme_dir, G_FILE_TEST_IS_DIR))
				++n_themes;

			g_free (theme_dir);
		}
		g_dir_rewind (dir);

		ok = TRUE;
		for (name = g_dir_read_name (dir); name && ok;
		     name = g_dir_read_name (dir)) {
			gchar *theme_dir;

			theme_dir = g_build_filename (tmp_dir, name, NULL);

			if (g_file_test (theme_dir, G_FILE_TEST_IS_DIR))
				ok = cafe_theme_install_real (parent,
							       theme_dir,
							       name,
							       n_themes == 1);

			g_free (theme_dir);
		}
		g_dir_close (dir);

		if (ok && n_themes > 1) {
			dialog = ctk_message_dialog_new (parent,
							 CTK_DIALOG_MODAL,
							 CTK_MESSAGE_INFO,
							 CTK_BUTTONS_OK,
							 _("New themes have been successfully installed."));
			ctk_dialog_run (CTK_DIALOG (dialog));
			ctk_widget_destroy (dialog);
		}
		g_io_scheduler_push_job ((GIOSchedulerJobFunc) cleanup_tmp_dir,
					 tmp_dir, g_free,
					 G_PRIORITY_DEFAULT, NULL);
	}
}

typedef struct
{
	CtkWindow *parent;
	char      *path;
} TransferData;

static void
transfer_done_cb (CtkWidget *dialog,
		  TransferData *tdata)
{
	/* XXX: path should be on the local filesystem by now? */

	if (dialog != NULL) {
		ctk_widget_destroy (dialog);
	}

	process_local_theme (tdata->parent, tdata->path);

	g_free (tdata->path);
	g_free (tdata);
}

void
cafe_theme_install (GFile *file,
		     CtkWindow *parent)
{
	CtkWidget *dialog;
	gchar *path, *base;
	GList *src, *target;
	const gchar *template;
	TransferData *tdata;

	if (file == NULL) {
		dialog = ctk_message_dialog_new (parent,
						 CTK_DIALOG_MODAL,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_OK,
						 _("No theme file location specified to install"));
		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		return;
	}

	/* see if someone dropped a local directory */
	if (g_file_is_native (file)
	    && g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY) {
		path = g_file_get_path (file);
		process_local_theme (parent, path);
		g_free (path);
		return;
	}

	/* we can't tell if this is an icon theme yet, so just make a
	 * temporary copy in .themes */
	path = g_build_filename (g_get_home_dir (), ".themes", NULL);

	if (access (path, X_OK | W_OK) != 0) {
		dialog = ctk_message_dialog_new (parent,
						 CTK_DIALOG_MODAL,
						 CTK_MESSAGE_ERROR,
						 CTK_BUTTONS_OK,
						 _("Insufficient permissions to install the theme in:\n%s"), path);
		ctk_dialog_run (CTK_DIALOG (dialog));
		ctk_widget_destroy (dialog);
		g_free (path);
		return;
	}

	g_free (path);

	base = g_file_get_basename (file);

	if (g_str_has_suffix (base, ".tar.gz")
	    || g_str_has_suffix (base, ".tgz")
	    || g_str_has_suffix (base, ".gtp"))
		template = "cafe-theme-%d.gtp";
	else if (g_str_has_suffix (base, ".tar.bz2"))
		template = "cafe-theme-%d.tar.bz2";
	else if (g_str_has_suffix (base, ".tar.xz"))
		template = "cafe-theme-%d.tar.xz";
	else {
		invalid_theme_dialog (parent, base, FALSE);
		g_free (base);
		return;
	}
	g_free (base);

	src = g_list_append (NULL, g_object_ref (file));

	path = NULL;
	do {
	  	gchar *file_tmp;

		g_free (path);
    		file_tmp = g_strdup_printf (template, g_random_int ());
	    	path = g_build_filename (g_get_home_dir (), ".themes", file_tmp, NULL);
	  	g_free (file_tmp);
	} while (g_file_test (path, G_FILE_TEST_EXISTS));

	tdata = g_new0 (TransferData, 1);
	tdata->parent = parent;
	tdata->path = path;

	dialog = file_transfer_dialog_new_with_parent (parent);
	g_signal_connect (dialog,
			  "cancel",
			  (GCallback) transfer_cancel_cb, path);
	g_signal_connect (dialog,
			  "done",
			  (GCallback) transfer_done_cb, tdata);

	target = g_list_append (NULL, g_file_new_for_path (path));
	file_transfer_dialog_copy_async (FILE_TRANSFER_DIALOG (dialog),
					 src,
					 target,
					 FILE_TRANSFER_DIALOG_DEFAULT,
					 G_PRIORITY_DEFAULT);
	ctk_widget_show (dialog);

	/* don't free the path since we're using that for the signals */
	g_list_foreach (src, (GFunc) g_object_unref, NULL);
	g_list_free (src);
	g_list_foreach (target, (GFunc) g_object_unref, NULL);
	g_list_free (target);
}

void
cafe_theme_installer_run (CtkWindow *parent)
{
	static gboolean running_theme_install = FALSE;
	static gchar old_folder[512] = "";
	CtkWidget *dialog;
	CtkFileFilter *filter;

	if (running_theme_install)
		return;

	running_theme_install = TRUE;

	dialog = ctk_file_chooser_dialog_new (_("Select Theme"),
					      parent,
					      CTK_FILE_CHOOSER_ACTION_OPEN,
					      "ctk-cancel",
					      CTK_RESPONSE_REJECT,
					      "ctk-open",
					      CTK_RESPONSE_ACCEPT,
					      NULL);
	ctk_dialog_set_default_response (CTK_DIALOG (dialog), CTK_RESPONSE_ACCEPT);

	filter = ctk_file_filter_new ();
	ctk_file_filter_set_name (filter, _("Theme Packages"));
	ctk_file_filter_add_mime_type (filter, "application/x-bzip-compressed-tar");
	ctk_file_filter_add_mime_type (filter, "application/x-xz-compressed-tar");
	ctk_file_filter_add_mime_type (filter, "application/x-compressed-tar");
	ctk_file_filter_add_mime_type (filter, "application/x-cafe-theme-package");
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (dialog), filter);

	filter = ctk_file_filter_new ();
	ctk_file_filter_set_name (filter, _("All Files"));
	ctk_file_filter_add_pattern(filter, "*");
	ctk_file_chooser_add_filter (CTK_FILE_CHOOSER (dialog), filter);

	if (strcmp (old_folder, ""))
		ctk_file_chooser_set_current_folder (CTK_FILE_CHOOSER (dialog), old_folder);

	if (ctk_dialog_run (CTK_DIALOG (dialog)) == CTK_RESPONSE_ACCEPT) {
		gchar *uri_selected, *folder;

		uri_selected = ctk_file_chooser_get_uri (CTK_FILE_CHOOSER (dialog));

		folder = ctk_file_chooser_get_current_folder (CTK_FILE_CHOOSER (dialog));
		g_strlcpy (old_folder, folder, 255);
		g_free (folder);

		ctk_widget_destroy (dialog);

		if (uri_selected != NULL) {
			GFile *file = g_file_new_for_uri (uri_selected);
			g_free (uri_selected);

			cafe_theme_install (file, parent);
			g_object_unref (file);
		}
	} else {
		ctk_widget_destroy (dialog);
	}

	/*
	 * we're relying on the cafe theme info module to pick up changes
	 * to the themes so we don't need to update the model here
	 */

	running_theme_install = FALSE;
}
