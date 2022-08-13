#ifndef CAFE_WINDOW_MANAGER_LIST_H
#define CAFE_WINDOW_MANAGER_LIST_H

#include <ctk/ctk.h>

#include "cafe-window-manager.h"

void cafe_wm_manager_init (void);

/* gets the currently active window manager */
CafeWindowManager *cafe_wm_manager_get_current (GdkScreen *screen);

gboolean cafe_wm_manager_spawn_config_tool_for_current (GdkScreen  *screen,
                                                         GError    **error);

#endif
