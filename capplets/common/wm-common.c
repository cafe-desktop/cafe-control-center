#include <X11/Xatom.h>
#include <cdk/cdkx.h>
#include <cdk/cdk.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "wm-common.h"

typedef struct _WMCallbackData
{
  GFunc func;
  gpointer data;
} WMCallbackData;

/* Our WM Window */
static Window wm_window = None;

static char *
wm_common_get_window_manager_property (Atom atom)
{
  Atom utf8_string, type;
  CdkDisplay *display;
  int result;
  char *retval;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;

  if (wm_window == None)
    return NULL;

  utf8_string = cdk_x11_get_xatom_by_name ("UTF8_STRING");

  display = cdk_display_get_default ();
  cdk_x11_display_error_trap_push (display);

  val = NULL;
  result = XGetWindowProperty (CDK_DISPLAY_XDISPLAY(display),
		  	       wm_window,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **) &val);

  if (cdk_x11_display_error_trap_pop (display) || result != Success ||
      type != utf8_string || format != 8 || nitems == 0 ||
      !g_utf8_validate (val, nitems, NULL))
    {
      retval = NULL;
    }
  else
    {
      retval = g_strndup (val, nitems);
    }

  if (val)
    XFree (val);

  return retval;
}

char*
wm_common_get_current_window_manager (void)
{
  Atom atom = cdk_x11_get_xatom_by_name ("_NET_WM_NAME");
  char *result;

  result = wm_common_get_window_manager_property (atom);
  if (result)
    return result;
  else
    return g_strdup (WM_COMMON_UNKNOWN);
}

char**
wm_common_get_current_keybindings (void)
{
  Atom keybindings_atom = cdk_x11_get_xatom_by_name ("_CAFE_WM_KEYBINDINGS");
  char *keybindings = wm_common_get_window_manager_property (keybindings_atom);
  char **results;

  if (keybindings)
    {
      char **p;
      results = g_strsplit(keybindings, ",", -1);
      for (p = results; *p; p++)
	g_strstrip (*p);
      g_free (keybindings);
    }
  else
    {
      Atom wm_atom = cdk_x11_get_xatom_by_name ("_NET_WM_NAME");
      char *wm_name = wm_common_get_window_manager_property (wm_atom);
      char *to_copy[] = { NULL, NULL };

      to_copy[0] = wm_name ? wm_name : WM_COMMON_UNKNOWN;

      results = g_strdupv (to_copy);
      g_free (wm_name);
    }

  return results;
}

static void
update_wm_window (void)
{
  CdkDisplay *display;
  Window *xwindow;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;

  display = cdk_display_get_default ();
  XGetWindowProperty (CDK_DISPLAY_XDISPLAY(display), CDK_ROOT_WINDOW (),
		      cdk_x11_get_xatom_by_name ("_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &nitems, &bytes_after, (guchar **) &xwindow);

  if (type != XA_WINDOW)
    {
      wm_window = None;
     return;
    }

  cdk_x11_display_error_trap_push (display);
  XSelectInput (CDK_DISPLAY_XDISPLAY(display), *xwindow, StructureNotifyMask | PropertyChangeMask);
  XSync (CDK_DISPLAY_XDISPLAY(display), False);

  if (cdk_x11_display_error_trap_pop (display))
    {
       XFree (xwindow);
       wm_window = None;
       return;
    }

    wm_window = *xwindow;
    XFree (xwindow);
}

static CdkFilterReturn
wm_window_event_filter (CdkXEvent *xev,
			CdkEvent  *event,
			gpointer   data)
{
  WMCallbackData *ncb_data = (WMCallbackData*) data;
  XEvent *xevent = (XEvent *)xev;

  if ((xevent->type == DestroyNotify &&
       wm_window != None && xevent->xany.window == wm_window) ||
      (xevent->type == PropertyNotify &&
       xevent->xany.window == CDK_ROOT_WINDOW () &&
       xevent->xproperty.atom == (cdk_x11_get_xatom_by_name ("_NET_SUPPORTING_WM_CHECK"))) ||
      (xevent->type == PropertyNotify &&
       wm_window != None && xevent->xany.window == wm_window &&
       xevent->xproperty.atom == (cdk_x11_get_xatom_by_name ("_NET_WM_NAME"))))
    {
      update_wm_window ();
      (* ncb_data->func) ((gpointer)wm_common_get_current_window_manager(),
		   	  ncb_data->data);
    }

  return CDK_FILTER_CONTINUE;
}

void
wm_common_register_window_manager_change (GFunc    func,
					  gpointer data)
{
  WMCallbackData *ncb_data;

  ncb_data = g_new0 (WMCallbackData, 1);

  ncb_data->func = func;
  ncb_data->data = data;

  cdk_window_add_filter (NULL, wm_window_event_filter, ncb_data);

  update_wm_window ();

  XSelectInput (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()), CDK_ROOT_WINDOW (), PropertyChangeMask);
  XSync (CDK_DISPLAY_XDISPLAY(cdk_display_get_default()), False);
}

void
wm_common_update_window ()
{
  update_wm_window();
}
