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

#include "cafe-window-manager.h"

#include <gmodule.h>

static GObjectClass *parent_class;

struct _CafeWindowManagerPrivate {
        char *window_manager_name;
        CafeDesktopItem *ditem;
};

GObject *
cafe_window_manager_new (CafeDesktopItem *it)
{
        const char *settings_lib;
        char *module_name;
        CafeWindowManagerNewFunc wm_new_func = NULL;
        GObject *wm;
        GModule *module;
        gboolean success;

        settings_lib = cafe_desktop_item_get_string (it, "X-MATE-WMSettingsModule");

        module_name = g_module_build_path (MATE_WINDOW_MANAGER_MODULE_PATH,
                                           settings_lib);

        module = g_module_open (module_name, G_MODULE_BIND_LAZY);
        if (module == NULL) {
                g_warning ("Couldn't load window manager settings module `%s' (%s)", module_name, g_module_error ());
		g_free (module_name);
                return NULL;
        }

        success = g_module_symbol (module, "window_manager_new",
                                   (gpointer *) &wm_new_func);

        if ((!success) || wm_new_func == NULL) {
                g_warning ("Couldn't load window manager settings module `%s`, couldn't find symbol \'window_manager_new\'", module_name);
		g_free (module_name);
                return NULL;
        }

	g_free (module_name);

        wm = (* wm_new_func) (MATE_WINDOW_MANAGER_INTERFACE_VERSION);

        if (wm == NULL)
                return NULL;

        (MATE_WINDOW_MANAGER (wm))->p->window_manager_name = g_strdup (cafe_desktop_item_get_string (it, MATE_DESKTOP_ITEM_NAME));
        (MATE_WINDOW_MANAGER (wm))->p->ditem = cafe_desktop_item_ref (it);

        return wm;
}

const char *
cafe_window_manager_get_name (CafeWindowManager *wm)
{
        return wm->p->window_manager_name;
}

CafeDesktopItem *
cafe_window_manager_get_ditem (CafeWindowManager *wm)
{
        return cafe_desktop_item_ref (wm->p->ditem);
}

GList *
cafe_window_manager_get_theme_list (CafeWindowManager *wm)
{
        CafeWindowManagerClass *klass = MATE_WINDOW_MANAGER_GET_CLASS (wm);
        if (klass->get_theme_list)
                return klass->get_theme_list (wm);
        else
                return NULL;
}

char *
cafe_window_manager_get_user_theme_folder (CafeWindowManager *wm)
{
        CafeWindowManagerClass *klass = MATE_WINDOW_MANAGER_GET_CLASS (wm);
        if (klass->get_user_theme_folder)
                return klass->get_user_theme_folder (wm);
        else
                return NULL;
}

void
cafe_window_manager_get_double_click_actions (CafeWindowManager              *wm,
                                               const CafeWMDoubleClickAction **actions,
                                               int                             *n_actions)
{
        CafeWindowManagerClass *klass = MATE_WINDOW_MANAGER_GET_CLASS (wm);

        *actions = NULL;
        *n_actions = 0;

        if (klass->get_double_click_actions)
                klass->get_double_click_actions (wm, actions, n_actions);
}

void
cafe_window_manager_change_settings  (CafeWindowManager    *wm,
                                       const CafeWMSettings *settings)
{
        CafeWindowManagerClass *klass = MATE_WINDOW_MANAGER_GET_CLASS (wm);

        (* klass->change_settings) (wm, settings);
}

void
cafe_window_manager_get_settings (CafeWindowManager *wm,
                                   CafeWMSettings    *settings)
{
        CafeWindowManagerClass *klass = MATE_WINDOW_MANAGER_GET_CLASS (wm);
        int mask;

        mask = (* klass->get_settings_mask) (wm);
        settings->flags &= mask; /* avoid back compat issues by not returning
                                  * fields to the caller that the WM module
                                  * doesn't know about
                                  */

        (* klass->get_settings) (wm, settings);
}

static void
cafe_window_manager_init (CafeWindowManager *cafe_window_manager, CafeWindowManagerClass *class)
{
	cafe_window_manager->p = g_new0 (CafeWindowManagerPrivate, 1);
}

static void
cafe_window_manager_finalize (GObject *object)
{
	CafeWindowManager *cafe_window_manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MATE_WINDOW_MANAGER (object));

	cafe_window_manager = MATE_WINDOW_MANAGER (object);

	g_free (cafe_window_manager->p);

	parent_class->finalize (object);
}

enum {
  SETTINGS_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
cafe_window_manager_class_init (CafeWindowManagerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = cafe_window_manager_finalize;

	parent_class = g_type_class_peek_parent (class);


        signals[SETTINGS_CHANGED] =
                g_signal_new ("settings_changed",
                              G_OBJECT_CLASS_TYPE (class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                              G_STRUCT_OFFSET (CafeWindowManagerClass, settings_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}


GType
cafe_window_manager_get_type (void)
{
	static GType cafe_window_manager_type = 0;

	if (!cafe_window_manager_type) {
		static GTypeInfo cafe_window_manager_info = {
			sizeof (CafeWindowManagerClass),
			NULL, /* GBaseInitFunc */
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) cafe_window_manager_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (CafeWindowManager),
			0, /* n_preallocs */
			(GInstanceInitFunc) cafe_window_manager_init,
			NULL
		};

		cafe_window_manager_type =
			g_type_register_static (G_TYPE_OBJECT,
						"CafeWindowManager",
						&cafe_window_manager_info, 0);
	}

	return cafe_window_manager_type;
}


void
cafe_window_manager_settings_changed (CafeWindowManager *wm)
{
        g_signal_emit (wm, signals[SETTINGS_CHANGED], 0);
}

/* Helper functions for CafeWMSettings */
CafeWMSettings *
cafe_wm_settings_copy (CafeWMSettings *settings)
{
        CafeWMSettings *retval;

        g_return_val_if_fail (settings != NULL, NULL);

        retval = g_new (CafeWMSettings, 1);
        *retval = *settings;

        if (retval->flags & MATE_WM_SETTING_FONT)
                retval->font = g_strdup (retval->font);
        if (retval->flags & MATE_WM_SETTING_MOUSE_MOVE_MODIFIER)
                retval->mouse_move_modifier = g_strdup (retval->mouse_move_modifier);
        if (retval->flags & MATE_WM_SETTING_THEME)
                retval->theme = g_strdup (retval->theme);

        return retval;
}

void
cafe_wm_settings_free (CafeWMSettings *settings)
{
        g_return_if_fail (settings != NULL);

        if (settings->flags & MATE_WM_SETTING_FONT)
                g_free ((void *) settings->font);
        if (settings->flags & MATE_WM_SETTING_MOUSE_MOVE_MODIFIER)
                g_free ((void *) settings->mouse_move_modifier);
        if (settings->flags & MATE_WM_SETTING_THEME)
                g_free ((void *)settings->theme);

        g_free (settings);
}

