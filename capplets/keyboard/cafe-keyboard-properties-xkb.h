/* -*- mode: c; style: linux -*- */

/* cafe-keyboard-properties-xkb.h
 * Copyright (C) 2003-2007 Sergey V Udaltsov
 *
 * Written by Sergey V. Udaltsov <svu@gnome.org>
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

#ifndef __CAFE_KEYBOARD_PROPERTY_XKB_H
#define __CAFE_KEYBOARD_PROPERTY_XKB_H

#include <gio/gio.h>

#include "libcafekbd/cafekbd-keyboard-config.h"

#ifdef __cplusplus
extern "C" {
#endif
#define CWID(s) GTK_WIDGET (ctk_builder_get_object (chooser_dialog, s))
extern XklEngine *engine;
extern XklConfigRegistry *config_registry;
extern GSettings *xkb_kbd_settings;
extern GSettings *xkb_general_settings;
extern CafekbdKeyboardConfig initial_config;

extern void setup_xkb_tabs (CtkBuilder * dialog);

extern void xkb_layouts_fill_selected_tree (CtkBuilder * dialog);

extern void xkb_layouts_register_buttons_handlers (CtkBuilder * dialog);

extern void xkb_layouts_register_gsettings_listener (CtkBuilder * dialog);

extern void xkb_options_register_gsettings_listener (CtkBuilder * dialog);

extern void xkb_layouts_prepare_selected_tree (CtkBuilder * dialog);

extern void xkb_options_load_options (CtkBuilder * dialog);

extern void xkb_options_popup_dialog (CtkBuilder * dialog);

extern void clear_xkb_elements_list (GSList * list);

extern char *xci_desc_to_utf8 (XklConfigItem * ci);

extern gchar *xkb_layout_description_utf8 (const gchar * visible);

extern void enable_disable_restoring (CtkBuilder * dialog);

extern void preview_toggled (CtkBuilder * dialog, CtkWidget * button);

extern void choose_model (CtkBuilder * dialog);

extern void xkb_layout_choose (CtkBuilder * dialog);

extern GSList *xkb_layouts_get_selected_list (void);

extern GSList *xkb_options_get_selected_list (void);

extern void xkb_layouts_set_selected_list(GSList *list);

extern void xkb_options_set_selected_list(GSList *list);

extern CtkWidget *xkb_layout_preview_create_widget (CtkBuilder *
						    chooser_dialog);

extern void xkb_layout_preview_update (CtkBuilder * chooser_dialog);

extern void xkb_layout_preview_set_drawing_layout (CtkWidget * kbdraw,
						   const gchar * id);

extern gchar *xkb_layout_chooser_get_selected_id (CtkBuilder *
						  chooser_dialog);

extern void xkb_save_default_group (gint group_no);

extern gint xkb_get_default_group (void);

#ifdef __cplusplus
}
#endif
#endif				/* __CAFE_KEYBOARD_PROPERTY_XKB_H */
