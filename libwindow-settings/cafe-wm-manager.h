#ifndef MATE_WINDOW_MANAGER_LIST_H
#define MATE_WINDOW_MANAGER_LIST_H

#include <gtk/gtk.h>

#include "cafe-window-manager.h"

void cafe_wm_manager_init (void);

/* gets the currently active window manager */
MateWindowManager *cafe_wm_manager_get_current (GdkScreen *screen);

gboolean cafe_wm_manager_spawn_config_tool_for_current (GdkScreen  *screen,
                                                         GError    **error);

#endif
