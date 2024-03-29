#ifndef __THEME_THUMBNAIL_H__
#define __THEME_THUMBNAIL_H__


#include <ctk/ctk.h>
#include "cafe-theme-info.h"

typedef void (* ThemeThumbnailFunc)          (GdkPixbuf          *pixbuf,
                                              gchar              *theme_name,
                                              gpointer            data);

GdkPixbuf *generate_meta_theme_thumbnail     (CafeThemeMetaInfo *theme_info);
GdkPixbuf *generate_ctk_theme_thumbnail      (CafeThemeInfo     *theme_info);
GdkPixbuf *generate_croma_theme_thumbnail (CafeThemeInfo     *theme_info);
GdkPixbuf *generate_icon_theme_thumbnail     (CafeThemeIconInfo *theme_info);

void generate_meta_theme_thumbnail_async     (CafeThemeMetaInfo *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);
void generate_ctk_theme_thumbnail_async      (CafeThemeInfo     *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);
void generate_croma_theme_thumbnail_async (CafeThemeInfo     *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);
void generate_icon_theme_thumbnail_async     (CafeThemeIconInfo *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);

void theme_thumbnail_factory_init            (int                 argc,
                                              char               *argv[]);

#endif /* __THEME_THUMBNAIL_H__ */
