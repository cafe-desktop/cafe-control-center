#ifndef __LIBSLAB_UTILS_H__
#define __LIBSLAB_UTILS_H__

#include <glib.h>
#include <ctk/ctk.h>
#include <libcafe-desktop/cafe-desktop-item.h>

#ifdef __cplusplus
extern "C" {
#endif

CafeDesktopItem *libslab_cafe_desktop_item_new_from_unknown_id (const gchar *id);
guint32           libslab_get_current_time_millis (void);
gint              libslab_strcmp (const gchar *a, const gchar *b);

CdkScreen *libslab_get_current_screen (void);

#ifdef __cplusplus
}
#endif

#endif
