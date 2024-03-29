#include "libslab-utils.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <ctk/ctk.h>

CafeDesktopItem *
libslab_cafe_desktop_item_new_from_unknown_id (const gchar *id)
{
	CafeDesktopItem *item;
	gchar            *basename;

	GError *error = NULL;


	if (! id)
		return NULL;

	item = cafe_desktop_item_new_from_uri (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	item = cafe_desktop_item_new_from_file (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	item = cafe_desktop_item_new_from_basename (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	basename = g_strrstr (id, "/");

	if (basename) {
		basename++;

		item = cafe_desktop_item_new_from_basename (basename, 0, &error);

		if (! error)
			return item;
		else {
			g_error_free (error);
			error = NULL;
		}
	}

	return NULL;
}

/* Ugh, here we don't have knowledge of the screen that is being used.  So, do
 * what we can to find it.
 */
CdkScreen *
libslab_get_current_screen (void)
{
	CdkEvent *event;
	CdkScreen *screen = NULL;

	event = ctk_get_current_event ();
	if (event) {
		if (event->any.window)
			screen = ctk_window_get_screen (CTK_WINDOW (event->any.window));

		cdk_event_free (event);
	}

	if (!screen)
		screen = cdk_screen_get_default ();

	return screen;
}

guint32
libslab_get_current_time_millis ()
{
	GTimeVal t_curr;

	g_get_current_time (& t_curr);

	return 1000L * t_curr.tv_sec + t_curr.tv_usec / 1000L;
}

gint
libslab_strcmp (const gchar *a, const gchar *b)
{
	if (! a && ! b)
		return 0;

	if (! a)
		return strcmp ("", b);

	if (! b)
		return strcmp (a, "");

	return strcmp (a, b);
}
