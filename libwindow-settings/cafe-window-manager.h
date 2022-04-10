/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* cafe-window-manager.h
 * Copyright (C) 2002 Seth Nickell
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * Written by: Seth Nickell <snickell@stanford.edu>,
 *             Havoc Pennington <hp@redhat.com>
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

#ifndef CAFE_WINDOW_MANAGER_H
#define CAFE_WINDOW_MANAGER_H

#include <glib-object.h>

#include <libcafe-desktop/cafe-desktop-item.h>

/* Increment if backward-incompatible changes are made, so we get a clean
 * error. In principle the libtool versioning handles this, but
 * in combination with dlopen I don't quite trust that.
 */
#define CAFE_WINDOW_MANAGER_INTERFACE_VERSION 1

typedef GObject * (* CafeWindowManagerNewFunc) (int expected_interface_version);

typedef enum
{
        CAFE_WM_SETTING_FONT                = 1 << 0,
        CAFE_WM_SETTING_MOUSE_FOCUS         = 1 << 1,
        CAFE_WM_SETTING_AUTORAISE           = 1 << 2,
        CAFE_WM_SETTING_AUTORAISE_DELAY     = 1 << 3,
        CAFE_WM_SETTING_MOUSE_MOVE_MODIFIER = 1 << 4,
        CAFE_WM_SETTING_THEME               = 1 << 5,
        CAFE_WM_SETTING_DOUBLE_CLICK_ACTION = 1 << 6,
        CAFE_WM_SETTING_COMPOSITING_MANAGER = 1 << 7,
        CAFE_WM_SETTING_COMPOSITING_ALTTAB  = 1 << 8,
        CAFE_WM_SETTING_MASK                =
        CAFE_WM_SETTING_FONT                |
        CAFE_WM_SETTING_MOUSE_FOCUS         |
        CAFE_WM_SETTING_AUTORAISE           |
        CAFE_WM_SETTING_AUTORAISE_DELAY     |
        CAFE_WM_SETTING_MOUSE_MOVE_MODIFIER |
        CAFE_WM_SETTING_THEME               |
        CAFE_WM_SETTING_DOUBLE_CLICK_ACTION |
        CAFE_WM_SETTING_COMPOSITING_MANAGER |
        CAFE_WM_SETTING_COMPOSITING_ALTTAB
} CafeWMSettingsFlags;

typedef struct
{
        int number;
        const char *human_readable_name;
} CafeWMDoubleClickAction;

typedef struct
{
        CafeWMSettingsFlags flags; /* this allows us to expand the struct
                                     * while remaining binary compatible
                                     */
        const char *font;
        int autoraise_delay;
        /* One of the strings "Alt", "Control", "Super", "Hyper", "Meta" */
        const char *mouse_move_modifier;
        const char *theme;
        int double_click_action;

        guint focus_follows_mouse : 1;
        guint autoraise : 1;

        gboolean compositing_manager;
        gboolean compositing_fast_alt_tab;

} CafeWMSettings;

#ifdef __cplusplus
extern "C" {
#endif

#define CAFE_WINDOW_MANAGER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, cafe_window_manager_get_type (), CafeWindowManager)
#define CAFE_WINDOW_MANAGER_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, cafe_window_manager_get_type (), CafeWindowManagerClass)
#define IS_CAFE_WINDOW_MANAGER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, cafe_window_manager_get_type ())
#define CAFE_WINDOW_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), cafe_window_manager_get_type, CafeWindowManagerClass))

typedef struct _CafeWindowManager CafeWindowManager;
typedef struct _CafeWindowManagerClass CafeWindowManagerClass;

typedef struct _CafeWindowManagerPrivate CafeWindowManagerPrivate;

struct _CafeWindowManager
{
        GObject parent;

        CafeWindowManagerPrivate *p;
};

struct _CafeWindowManagerClass
{
        GObjectClass klass;

        void         (* settings_changed)       (CafeWindowManager    *wm);

        void         (* change_settings)        (CafeWindowManager    *wm,
                                                 const CafeWMSettings *settings);
        void         (* get_settings)           (CafeWindowManager    *wm,
                                                 CafeWMSettings       *settings);

        GList *      (* get_theme_list)         (CafeWindowManager *wm);
        char *       (* get_user_theme_folder)  (CafeWindowManager *wm);

        int          (* get_settings_mask)      (CafeWindowManager *wm);

        void         (* get_double_click_actions) (CafeWindowManager              *wm,
                                                   const CafeWMDoubleClickAction **actions,
                                                   int                             *n_actions);

        void         (* padding_func_1)         (CafeWindowManager *wm);
        void         (* padding_func_2)         (CafeWindowManager *wm);
        void         (* padding_func_3)         (CafeWindowManager *wm);
        void         (* padding_func_4)         (CafeWindowManager *wm);
        void         (* padding_func_5)         (CafeWindowManager *wm);
        void         (* padding_func_6)         (CafeWindowManager *wm);
        void         (* padding_func_7)         (CafeWindowManager *wm);
        void         (* padding_func_8)         (CafeWindowManager *wm);
        void         (* padding_func_9)         (CafeWindowManager *wm);
        void         (* padding_func_10)        (CafeWindowManager *wm);
};

GObject *         cafe_window_manager_new                     (CafeDesktopItem   *item);
GType             cafe_window_manager_get_type                (void);
const char *      cafe_window_manager_get_name                (CafeWindowManager *wm);
CafeDesktopItem *cafe_window_manager_get_ditem               (CafeWindowManager *wm);

/* GList of char *'s */
GList *           cafe_window_manager_get_theme_list          (CafeWindowManager *wm);
char *            cafe_window_manager_get_user_theme_folder   (CafeWindowManager *wm);

/* only uses fields with their flags set */
void              cafe_window_manager_change_settings  (CafeWindowManager    *wm,
                                                         const CafeWMSettings *settings);
/* only gets fields with their flags set (and if it fails to get a field,
 * it unsets that flag, so flags should be checked on return)
 */
void              cafe_window_manager_get_settings     (CafeWindowManager *wm,
                                                         CafeWMSettings    *settings);

void              cafe_window_manager_settings_changed (CafeWindowManager *wm);

void cafe_window_manager_get_double_click_actions (CafeWindowManager              *wm,
                                                    const CafeWMDoubleClickAction **actions,
                                                    int                             *n_actions);

/* Helper functions for CafeWMSettings */
CafeWMSettings *cafe_wm_settings_copy (CafeWMSettings *settings);
void             cafe_wm_settings_free (CafeWMSettings *settings);

#ifdef __cplusplus
}
#endif

#endif /* CAFE_WINDOW_MANAGER_H */
