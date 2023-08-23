/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* cafe-window-properties.c
 * Copyright (C) 2002 Seth Nickell
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * Written by: Seth Nickell <snickell@stanford.edu>
 *             Havoc Pennington <hp@redhat.com>
 *             Stefano Karapetsas <stefano@karapetsas.com>
 *             Friedrich Herbst <frimam@web.de>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <glib/gi18n.h>
#include <string.h>

#include <cdk/cdkx.h>

#include "cafe-metacity-support.h"
#include "wm-common.h"
#include "capplet-util.h"

#define CROMA_SCHEMA "org.cafe.Croma.general"
#define CROMA_THEME_KEY "theme"
#define CROMA_FONT_KEY  "titlebar-font"
#define CROMA_FOCUS_KEY "focus-mode"
#define CROMA_USE_SYSTEM_FONT_KEY "titlebar-uses-system-font"
#define CROMA_AUTORAISE_KEY "auto-raise"
#define CROMA_AUTORAISE_DELAY_KEY "auto-raise-delay"
#define CROMA_MOUSE_MODIFIER_KEY "mouse-button-modifier"
#define CROMA_DOUBLE_CLICK_TITLEBAR_KEY "action-double-click-titlebar"
#define CROMA_COMPOSITING_MANAGER_KEY "compositing-manager"
#define CROMA_COMPOSITING_FAST_ALT_TAB_KEY "compositing-fast-alt-tab"
#define CROMA_ALLOW_TILING_KEY "allow-tiling"
#define CROMA_CENTER_NEW_WINDOWS_KEY "center-new-windows"
#define CROMA_BUTTON_LAYOUT_KEY "button-layout"

#define CROMA_BUTTON_LAYOUT_RIGHT "menu:minimize,maximize,close"
#define CROMA_BUTTON_LAYOUT_LEFT "close,minimize,maximize:"

/* keep following enums in sync with croma */
enum
{
    ACTION_TITLEBAR_TOGGLE_SHADE,
    ACTION_TITLEBAR_TOGGLE_MAXIMIZE,
    ACTION_TITLEBAR_TOGGLE_MAXIMIZE_HORIZONTALLY,
    ACTION_TITLEBAR_TOGGLE_MAXIMIZE_VERTICALLY,
    ACTION_TITLEBAR_MINIMIZE,
    ACTION_TITLEBAR_NONE,
    ACTION_TITLEBAR_LOWER,
    ACTION_TITLEBAR_MENU
};
enum
{
    FOCUS_MODE_CLICK,
    FOCUS_MODE_SLOPPY,
    FOCUS_MODE_MOUSE
};

typedef struct
{
    int number;
    char *name;
    const char *value; /* machine-readable name for storing config */
    CtkWidget *radio;
} MouseClickModifier;

static CtkWidget *dialog_win;
static CtkWidget *compositing_checkbutton;
static CtkWidget *compositing_fast_alt_tab_checkbutton;
static CtkWidget *allow_tiling_checkbutton;
static CtkWidget *center_new_windows_checkbutton;
static CtkWidget *focus_mode_checkbutton;
static CtkWidget *focus_mode_mouse_checkbutton;
static CtkWidget *autoraise_checkbutton;
static CtkWidget *autoraise_delay_slider;
static CtkWidget *autoraise_delay_hbox;
static CtkWidget *double_click_titlebar_optionmenu;
static CtkWidget *titlebar_layout_optionmenu;
static CtkWidget *alt_click_vbox;

static GSettings *croma_settings;

static MouseClickModifier *mouse_modifiers = NULL;
static int n_mouse_modifiers = 0;

static void reload_mouse_modifiers (void);

static void
update_sensitivity ()
{
    gchar *str;

    ctk_widget_set_sensitive (CTK_WIDGET (compositing_fast_alt_tab_checkbutton),
                              g_settings_get_boolean (croma_settings, CROMA_COMPOSITING_MANAGER_KEY));
    ctk_widget_set_sensitive (CTK_WIDGET (focus_mode_mouse_checkbutton),
                              g_settings_get_enum (croma_settings, CROMA_FOCUS_KEY) != FOCUS_MODE_CLICK);
    ctk_widget_set_sensitive (CTK_WIDGET (autoraise_checkbutton),
                              g_settings_get_enum (croma_settings, CROMA_FOCUS_KEY) != FOCUS_MODE_CLICK);
    ctk_widget_set_sensitive (CTK_WIDGET (autoraise_delay_hbox),
                              g_settings_get_enum (croma_settings, CROMA_FOCUS_KEY) != FOCUS_MODE_CLICK &&
                              g_settings_get_boolean (croma_settings, CROMA_AUTORAISE_KEY));

    str = g_settings_get_string (croma_settings, CROMA_BUTTON_LAYOUT_KEY);
    ctk_widget_set_sensitive (CTK_WIDGET (titlebar_layout_optionmenu),
                              g_strcmp0 (str, CROMA_BUTTON_LAYOUT_LEFT) == 0 ||
                              g_strcmp0 (str, CROMA_BUTTON_LAYOUT_RIGHT) == 0);
    g_free (str);
}

static void
croma_settings_changed_callback (GSettings *settings,
                                 const gchar *key,
                                 gpointer user_data)
{
    update_sensitivity ();
}

static void
mouse_focus_toggled_callback (CtkWidget *button,
                              void      *data)
{
    if (ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (focus_mode_checkbutton))) {
        g_settings_set_enum (croma_settings,
                             CROMA_FOCUS_KEY,
                             ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (focus_mode_mouse_checkbutton)) ?
                             FOCUS_MODE_MOUSE : FOCUS_MODE_SLOPPY);
    }
    else {
        g_settings_set_enum (croma_settings, CROMA_FOCUS_KEY, FOCUS_MODE_CLICK);
    }
}

static void
mouse_focus_changed_callback (GSettings *settings,
                              const gchar *key,
                              gpointer user_data)
{
    if (g_settings_get_enum (settings, key) == FOCUS_MODE_MOUSE) {
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (focus_mode_checkbutton), TRUE);
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (focus_mode_mouse_checkbutton), TRUE);
    }
    else if (g_settings_get_enum (settings, key) == FOCUS_MODE_SLOPPY) {
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (focus_mode_checkbutton), TRUE);
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (focus_mode_mouse_checkbutton), FALSE);
    }
    else {
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (focus_mode_checkbutton), FALSE);
        ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (focus_mode_mouse_checkbutton), FALSE);
    }
}

static void
autoraise_delay_value_changed_callback (CtkWidget *slider,
                                        void      *data)
{
    g_settings_set_int (croma_settings,
                        CROMA_AUTORAISE_DELAY_KEY,
                        ctk_range_get_value (CTK_RANGE (slider)) * 1000);
}

static void
double_click_titlebar_changed_callback (CtkWidget *optionmenu,
                                        void      *data)
{
    g_settings_set_enum (croma_settings, CROMA_DOUBLE_CLICK_TITLEBAR_KEY,
                         ctk_combo_box_get_active (CTK_COMBO_BOX (optionmenu)));
}

static void
titlebar_layout_changed_callback (CtkWidget *optionmenu,
                                  void      *data)
{
    gint value = ctk_combo_box_get_active (CTK_COMBO_BOX (optionmenu));

    if (value == 0) {
        g_settings_set_string (croma_settings, CROMA_BUTTON_LAYOUT_KEY, CROMA_BUTTON_LAYOUT_RIGHT);
    }
    else if (value == 1) {
        g_settings_set_string (croma_settings, CROMA_BUTTON_LAYOUT_KEY, CROMA_BUTTON_LAYOUT_LEFT);
    }
}

static void
alt_click_radio_toggled_callback (CtkWidget *radio,
                                  void      *data)
{
    MouseClickModifier *modifier = data;
    gboolean active;
    gchar *value;

    active = ctk_toggle_button_get_active (CTK_TOGGLE_BUTTON (radio));

    if (active) {
        value = g_strdup_printf ("<%s>", modifier->value);
        g_settings_set_string (croma_settings, CROMA_MOUSE_MODIFIER_KEY, value);
        g_free (value);
    }
}

static void
set_alt_click_value ()
{
    gboolean match_found = FALSE;
    gchar *mouse_move_modifier;
    gchar *value;
    int i;

    mouse_move_modifier = g_settings_get_string (croma_settings, CROMA_MOUSE_MODIFIER_KEY);

    /* We look for a matching modifier and set it. */
    if (mouse_move_modifier != NULL) {
        for (i = 0; i < n_mouse_modifiers; i ++) {
            value = g_strdup_printf ("<%s>", mouse_modifiers[i].value);
            if (strcmp (value, mouse_move_modifier) == 0) {
                ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (mouse_modifiers[i].radio), TRUE);
                match_found = TRUE;
                break;
            }
            g_free (value);
        }
        g_free (mouse_move_modifier);
    }

    /* No matching modifier was found; we set all the toggle buttons to be
     * insensitive. */
    for (i = 0; i < n_mouse_modifiers; i++) {
        ctk_toggle_button_set_inconsistent (CTK_TOGGLE_BUTTON (mouse_modifiers[i].radio), ! match_found);
    }
}

static void
wm_unsupported ()
{
    CtkWidget *no_tool_dialog;

    no_tool_dialog = ctk_message_dialog_new (NULL,
                                             CTK_DIALOG_DESTROY_WITH_PARENT,
                                             CTK_MESSAGE_ERROR,
                                             CTK_BUTTONS_CLOSE,
                                             " ");
    ctk_window_set_title (CTK_WINDOW (no_tool_dialog), "");
    ctk_window_set_resizable (CTK_WINDOW (no_tool_dialog), FALSE);

    ctk_message_dialog_set_markup (CTK_MESSAGE_DIALOG (no_tool_dialog), _("The current window manager is unsupported"));

    ctk_dialog_run (CTK_DIALOG (no_tool_dialog));

    ctk_widget_destroy (no_tool_dialog);
}

static void
wm_changed_callback (CdkScreen *screen,
                     void      *data)
{
    const char *current_wm;

    current_wm = cdk_x11_screen_get_window_manager_name (screen);

    ctk_widget_set_sensitive (dialog_win, g_strcmp0 (current_wm, WM_COMMON_CROMA) == 0);
}

static void
response_cb (CtkWidget *dialog_win,
             int    response_id,
             void      *data)
{

    if (response_id == CTK_RESPONSE_HELP) {
        capplet_help (CTK_WINDOW (dialog_win), "goscustdesk-58");
    } else {
        ctk_widget_destroy (dialog_win);
    }
}

CtkWidget*
title_label_new (const char* title)
{
    CtkWidget *widget;
    gchar *str;

    str = g_strdup_printf ("<b>%s</b>", _(title));
    widget = ctk_label_new (str);
    g_free (str);

    ctk_label_set_use_markup (CTK_LABEL (widget), TRUE);
    ctk_label_set_xalign (CTK_LABEL (widget), 0.0);
    ctk_label_set_yalign (CTK_LABEL (widget), 0.0);

    return widget;
}

int
main (int argc, char **argv)
{
    GError     *error = NULL;
    CtkBuilder *builder;
    CdkScreen  *screen;
    gchar      *str;
    const char *current_wm;
    int i;

    capplet_init (NULL, &argc, &argv);

    screen = cdk_display_get_default_screen (cdk_display_get_default ());
    current_wm = cdk_x11_screen_get_window_manager_name (screen);

    if (g_strcmp0 (current_wm, WM_COMMON_METACITY) == 0) {
        cafe_metacity_config_tool ();
        return 0;
    }

    if (g_strcmp0 (current_wm, WM_COMMON_CROMA) != 0) {
        wm_unsupported ();
        return 1;
    }

    croma_settings = g_settings_new (CROMA_SCHEMA);

    builder = ctk_builder_new ();
    if (ctk_builder_add_from_resource (builder, "/org/cafe/ccc/windows/window-properties.ui", &error) == 0) {
        g_warning ("Could not load UI: %s", error->message);
        g_error_free (error);
        g_object_unref (croma_settings);
        g_object_unref (builder);
        return -1;
    }

    ctk_builder_add_callback_symbols (builder,
                                      "on_dialog_win_response",                        G_CALLBACK (response_cb),
                                      "on_autoraise_delay_slider_value_changed",       G_CALLBACK (autoraise_delay_value_changed_callback),
                                      "on_double_click_titlebar_optionmenu_changed",   G_CALLBACK (double_click_titlebar_changed_callback),
                                      "on_titlebar_layout_optionmenu_changed",         G_CALLBACK (titlebar_layout_changed_callback),
                                      "on_focus_mode_checkbutton_toggled",             G_CALLBACK (mouse_focus_toggled_callback),
                                      "on_focus_mode_mouse_checkbutton_toggled",       G_CALLBACK (mouse_focus_toggled_callback),
                                      NULL);

    ctk_builder_connect_signals (builder, NULL);

    /* Window */
    dialog_win = CTK_WIDGET (ctk_builder_get_object (builder, "dialog_win"));

    /* Compositing manager */
    compositing_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "compositing_checkbutton"));
    compositing_fast_alt_tab_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "compositing_fast_alt_tab_checkbutton"));

    /* Titlebar buttons */
    titlebar_layout_optionmenu = CTK_WIDGET (ctk_builder_get_object (builder, "titlebar_layout_optionmenu"));

    /* New Windows */
    center_new_windows_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "center_new_windows_checkbutton"));

    /* Window Snapping */
    allow_tiling_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "allow_tiling_checkbutton"));

    /* Window Selection */
    focus_mode_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "focus_mode_checkbutton"));
    focus_mode_mouse_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "focus_mode_mouse_checkbutton"));
    autoraise_checkbutton = CTK_WIDGET (ctk_builder_get_object (builder, "autoraise_checkbutton"));
    autoraise_delay_hbox = CTK_WIDGET (ctk_builder_get_object (builder, "autoraise_delay_hbox"));
    autoraise_delay_slider = CTK_WIDGET (ctk_builder_get_object (builder, "autoraise_delay_slider"));

    /* Titlebar Action */
    double_click_titlebar_optionmenu = CTK_WIDGET (ctk_builder_get_object (builder, "double_click_titlebar_optionmenu"));

    /* Movement Key */
    alt_click_vbox = CTK_WIDGET (ctk_builder_get_object (builder, "alt_click_vbox"));


    g_object_unref (builder);


    reload_mouse_modifiers ();

    str = g_settings_get_string (croma_settings, CROMA_BUTTON_LAYOUT_KEY);
    ctk_combo_box_set_active (CTK_COMBO_BOX (titlebar_layout_optionmenu),
                              g_strcmp0 (str, CROMA_BUTTON_LAYOUT_RIGHT) == 0 ? 0 : 1);
    g_free (str);

    ctk_combo_box_set_active (CTK_COMBO_BOX (double_click_titlebar_optionmenu),
                              g_settings_get_enum (croma_settings, CROMA_DOUBLE_CLICK_TITLEBAR_KEY));

    set_alt_click_value ();
    ctk_range_set_value (CTK_RANGE (autoraise_delay_slider),
                         g_settings_get_int (croma_settings, CROMA_AUTORAISE_DELAY_KEY) / 1000.0);
    ctk_combo_box_set_active (CTK_COMBO_BOX (double_click_titlebar_optionmenu),
                              g_settings_get_enum (croma_settings, CROMA_DOUBLE_CLICK_TITLEBAR_KEY));

    g_settings_bind (croma_settings,
                     CROMA_COMPOSITING_MANAGER_KEY,
                     compositing_checkbutton,
                     "active",
                     G_SETTINGS_BIND_DEFAULT);

    g_settings_bind (croma_settings,
                     CROMA_COMPOSITING_FAST_ALT_TAB_KEY,
                     compositing_fast_alt_tab_checkbutton,
                     "active",
                     G_SETTINGS_BIND_DEFAULT);

    g_settings_bind (croma_settings,
                     CROMA_ALLOW_TILING_KEY,
                     allow_tiling_checkbutton,
                     "active",
                     G_SETTINGS_BIND_DEFAULT);

    g_settings_bind (croma_settings,
                     CROMA_CENTER_NEW_WINDOWS_KEY,
                     center_new_windows_checkbutton,
                     "active",
                     G_SETTINGS_BIND_DEFAULT);

    /* Initialize the checkbox state appropriately */
    mouse_focus_changed_callback(croma_settings, CROMA_FOCUS_KEY, NULL);

    g_settings_bind (croma_settings,
                     CROMA_AUTORAISE_KEY,
                     autoraise_checkbutton,
                     "active",
                     G_SETTINGS_BIND_DEFAULT);


    g_signal_connect (G_OBJECT (dialog_win), "destroy",
                      G_CALLBACK (ctk_main_quit), NULL);

    g_signal_connect (croma_settings, "changed",
                      G_CALLBACK (croma_settings_changed_callback), NULL);

    g_signal_connect (croma_settings, "changed::" CROMA_FOCUS_KEY,
                      G_CALLBACK (mouse_focus_changed_callback), NULL);

    g_signal_connect (G_OBJECT (screen), "window_manager_changed",
                      G_CALLBACK (wm_changed_callback), NULL);

    i = 0;
    while (i < n_mouse_modifiers) {
        g_signal_connect (G_OBJECT (mouse_modifiers[i].radio), "toggled",
                          G_CALLBACK (alt_click_radio_toggled_callback),
                          &mouse_modifiers[i]);
        ++i;
    }

    /* update sensitivity */
    update_sensitivity ();

    ctk_widget_show_all (dialog_win);

    ctk_main ();

    g_object_unref (croma_settings);

    return 0;
}

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cdk/cdkx.h>

static void
fill_radio (CtkRadioButton     *group,
        MouseClickModifier *modifier)
{
    modifier->radio = ctk_radio_button_new_with_mnemonic_from_widget (group, modifier->name);
    ctk_box_pack_start (CTK_BOX (alt_click_vbox), modifier->radio, FALSE, FALSE, 0);

    ctk_widget_show (modifier->radio);
}

static void
reload_mouse_modifiers (void)
{
    XModifierKeymap *modmap;
    KeySym *keymap;
    int keysyms_per_keycode;
    int map_size;
    int i;
    gboolean have_meta;
    gboolean have_hyper;
    gboolean have_super;
    int min_keycode, max_keycode;
    int mod_meta, mod_super, mod_hyper;

    XDisplayKeycodes (cdk_x11_display_get_xdisplay(cdk_display_get_default()),
                      &min_keycode,
                      &max_keycode);

    keymap = XGetKeyboardMapping (cdk_x11_display_get_xdisplay(cdk_display_get_default()),
                                  min_keycode,
                                  max_keycode - min_keycode,
                                  &keysyms_per_keycode);

    modmap = XGetModifierMapping (cdk_x11_display_get_xdisplay(cdk_display_get_default()));

    have_super = FALSE;
    have_meta = FALSE;
    have_hyper = FALSE;

    /* there are 8 modifiers, and the first 3 are shift, shift lock,
     * and control
     */
    map_size = 8 * modmap->max_keypermod;
    i = 3 * modmap->max_keypermod;
    mod_meta = mod_super = mod_hyper = 0;
    while (i < map_size) {
        /* get the key code at this point in the map,
         * see if its keysym is one we're interested in
         */
        int keycode = modmap->modifiermap[i];

        if (keycode >= min_keycode &&
            keycode <= max_keycode) {
            int j = 0;
            KeySym *syms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

            while (j < keysyms_per_keycode) {
                if (syms[j] == XK_Super_L || syms[j] == XK_Super_R)
                    mod_super = i / modmap->max_keypermod;
                else if (syms[j] == XK_Hyper_L || syms[j] == XK_Hyper_R)
                    mod_hyper = i / modmap->max_keypermod;
                else if ((syms[j] == XK_Meta_L || syms[j] == XK_Meta_R))
                    mod_meta = i / modmap->max_keypermod;
                ++j;
            }
        }

        ++i;
    }

    if ((1 << mod_meta) != Mod1Mask)
        have_meta = TRUE;
    if (mod_super != 0 && mod_super != mod_meta)
        have_super = TRUE;
    if (mod_hyper != 0 && mod_hyper != mod_meta && mod_hyper != mod_super)
        have_hyper = TRUE;

    XFreeModifiermap (modmap);
    XFree (keymap);

    i = 0;
    while (i < n_mouse_modifiers) {
        g_free (mouse_modifiers[i].name);
        if (mouse_modifiers[i].radio)
            ctk_widget_destroy (mouse_modifiers[i].radio);
        ++i;
    }
    g_free (mouse_modifiers);
    mouse_modifiers = NULL;

    n_mouse_modifiers = 1; /* alt */
    if (have_super)
        ++n_mouse_modifiers;
    if (have_hyper)
        ++n_mouse_modifiers;
    if (have_meta)
        ++n_mouse_modifiers;

    mouse_modifiers = g_new0 (MouseClickModifier, n_mouse_modifiers);

    i = 0;

    mouse_modifiers[i].number = i;
    mouse_modifiers[i].name = g_strdup (_("_Alt"));
    mouse_modifiers[i].value = "Alt";
    ++i;

    if (have_hyper) {
        mouse_modifiers[i].number = i;
        mouse_modifiers[i].name = g_strdup (_("H_yper"));
        mouse_modifiers[i].value = "Hyper";
        ++i;
    }

    if (have_super) {
        mouse_modifiers[i].number = i;
        mouse_modifiers[i].name = g_strdup (_("S_uper (or \"Windows logo\")"));
        mouse_modifiers[i].value = "Super";
        ++i;
    }

    if (have_meta) {
        mouse_modifiers[i].number = i;
        mouse_modifiers[i].name = g_strdup (_("_Meta"));
        mouse_modifiers[i].value = "Meta";
        ++i;
    }

    g_assert (i == n_mouse_modifiers);

    i = 0;
    while (i < n_mouse_modifiers) {
        fill_radio (i == 0 ? NULL : CTK_RADIO_BUTTON (mouse_modifiers[i-1].radio), &mouse_modifiers[i]);
        ++i;
    }
}
