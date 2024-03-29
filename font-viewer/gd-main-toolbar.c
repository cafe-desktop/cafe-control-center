/*
 * Copyright (c) 2011, 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "gd-main-toolbar.h"

#include <math.h>
#include <glib/gi18n.h>

typedef enum {
  CHILD_NORMAL = 0,
  CHILD_TOGGLE = 1,
  CHILD_MENU = 2,
} ChildType;

struct _GdMainToolbarPrivate {
  CtkSizeGroup *size_group;
  CtkSizeGroup *vertical_size_group;

  CtkToolItem *left_group;
  CtkToolItem *center_group;
  CtkToolItem *right_group;

  CtkWidget *left_grid;
  CtkWidget *center_grid;

  CtkWidget *labels_grid;
  CtkWidget *title_label;
  CtkWidget *detail_label;

  CtkWidget *modes_box;

  CtkWidget *center_menu;
  CtkWidget *center_menu_child;

  CtkWidget *right_grid;

  gboolean show_modes;
};

enum {
        PROP_0,
        PROP_SHOW_MODES,
};

G_DEFINE_TYPE_WITH_PRIVATE (GdMainToolbar, gd_main_toolbar, CTK_TYPE_TOOLBAR)

static void
gd_main_toolbar_dispose (GObject *obj)
{
  GdMainToolbar *self = GD_MAIN_TOOLBAR (obj);

  g_clear_object (&self->priv->size_group);
  g_clear_object (&self->priv->vertical_size_group);

  G_OBJECT_CLASS (gd_main_toolbar_parent_class)->dispose (obj);
}

static CtkWidget *
get_empty_button (ChildType type)
{
  CtkWidget *button;

  switch (type)
    {
    case CHILD_MENU:
      button = ctk_menu_button_new ();
      break;
    case CHILD_TOGGLE:
      button = ctk_toggle_button_new ();
      break;
    case CHILD_NORMAL:
    default:
      button = ctk_button_new ();
      break;
    }

  return button;
}

static CtkWidget *
get_symbolic_button (const gchar *icon_name,
                     ChildType    type)
{
  CtkWidget *button, *w;

  switch (type)
    {
    case CHILD_MENU:
      button = ctk_menu_button_new ();
      ctk_widget_destroy (ctk_bin_get_child (CTK_BIN (button)));
      break;
    case CHILD_TOGGLE:
      button = ctk_toggle_button_new ();
      break;
    case CHILD_NORMAL:
    default:
      button = ctk_button_new ();
      break;
    }

  ctk_style_context_add_class (ctk_widget_get_style_context (button), "raised");
  ctk_style_context_add_class (ctk_widget_get_style_context (button), "image-button");

  w = ctk_image_new_from_icon_name (icon_name, CTK_ICON_SIZE_MENU);
  ctk_widget_show (w);
  ctk_container_add (CTK_CONTAINER (button), w);

  return button;
}

static CtkWidget *
get_text_button (const gchar *label,
                 ChildType    type)
{
  CtkWidget *button, *w;

  switch (type)
    {
    case CHILD_MENU:
      button = ctk_menu_button_new ();
      ctk_widget_destroy (ctk_bin_get_child (CTK_BIN (button)));

      w = ctk_label_new (label);
      ctk_widget_show (w);
      ctk_container_add (CTK_CONTAINER (button), w);
      break;
    case CHILD_TOGGLE:
      button = ctk_toggle_button_new_with_label (label);
      break;
    case CHILD_NORMAL:
    default:
      button = ctk_button_new_with_label (label);
      break;
    }

  ctk_style_context_add_class (ctk_widget_get_style_context (button), "raised");
  ctk_style_context_add_class (ctk_widget_get_style_context (button), "text-button");

  return button;
}

static CtkSizeGroup *
get_vertical_size_group (GdMainToolbar *self)
{
  CtkSizeGroup *retval;
  CtkWidget *dummy;
  CtkToolItem *container;

  dummy = get_text_button ("Dummy", CHILD_NORMAL);
  container = ctk_tool_item_new ();
  ctk_widget_set_no_show_all (CTK_WIDGET (container), TRUE);
  ctk_container_add (CTK_CONTAINER (container), dummy);
  ctk_toolbar_insert (CTK_TOOLBAR (self), container, -1);

  retval = ctk_size_group_new (CTK_SIZE_GROUP_VERTICAL);
  ctk_size_group_add_widget (retval, dummy);

  return retval;
}

gboolean
gd_main_toolbar_get_show_modes (GdMainToolbar *self)
{
  return self->priv->show_modes;
}

void
gd_main_toolbar_set_show_modes (GdMainToolbar *self,
                                gboolean show_modes)
{
  if (self->priv->show_modes == show_modes)
    return;

  self->priv->show_modes = show_modes;
  if (self->priv->show_modes)
    {
      ctk_widget_set_no_show_all (self->priv->labels_grid, TRUE);
      ctk_widget_hide (self->priv->labels_grid);

      ctk_widget_set_valign (self->priv->center_grid, CTK_ALIGN_FILL);
      ctk_widget_set_no_show_all (self->priv->modes_box, FALSE);
      ctk_widget_show_all (self->priv->modes_box);
    }
  else
    {
      ctk_widget_set_no_show_all (self->priv->modes_box, TRUE);
      ctk_widget_hide (self->priv->modes_box);

      ctk_widget_set_valign (self->priv->center_grid, CTK_ALIGN_CENTER);
      ctk_widget_set_no_show_all (self->priv->labels_grid, FALSE);
      ctk_widget_show_all (self->priv->labels_grid);
    }

  g_object_notify (G_OBJECT (self), "show-modes");
}

static void
gd_main_toolbar_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{

  GdMainToolbar *self = GD_MAIN_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_SHOW_MODES:
      gd_main_toolbar_set_show_modes (GD_MAIN_TOOLBAR (self), g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gd_main_toolbar_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdMainToolbar *self = GD_MAIN_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_SHOW_MODES:
      g_value_set_boolean (value, self->priv->show_modes);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gd_main_toolbar_constructed (GObject *obj)
{
  GdMainToolbar *self = GD_MAIN_TOOLBAR (obj);
  CtkToolbar *tb = CTK_TOOLBAR (obj);
  CtkWidget *grid;

  G_OBJECT_CLASS (gd_main_toolbar_parent_class)->constructed (obj);

  self->priv->vertical_size_group = get_vertical_size_group (self);

  /* left section */
  self->priv->left_group = ctk_tool_item_new ();
  ctk_widget_set_margin_end (CTK_WIDGET (self->priv->left_group), 12);
  ctk_toolbar_insert (tb, self->priv->left_group, -1);
  ctk_size_group_add_widget (self->priv->vertical_size_group,
                             CTK_WIDGET (self->priv->left_group));

  /* left button group */
  self->priv->left_grid = ctk_grid_new ();
  ctk_grid_set_column_spacing (CTK_GRID (self->priv->left_grid), 12);
  ctk_container_add (CTK_CONTAINER (self->priv->left_group), self->priv->left_grid);
  ctk_widget_set_halign (self->priv->left_grid, CTK_ALIGN_START);

  /* center section */
  self->priv->center_group = ctk_tool_item_new ();
  ctk_tool_item_set_expand (self->priv->center_group, TRUE);
  ctk_toolbar_insert (tb, self->priv->center_group, -1);
  self->priv->center_grid = ctk_grid_new ();
  ctk_widget_set_halign (self->priv->center_grid, CTK_ALIGN_CENTER);
  ctk_widget_set_valign (self->priv->center_grid, CTK_ALIGN_CENTER);
  ctk_container_add (CTK_CONTAINER (self->priv->center_group), self->priv->center_grid);
  ctk_size_group_add_widget (self->priv->vertical_size_group,
                             CTK_WIDGET (self->priv->center_group));

  /* centered label group */
  self->priv->labels_grid = grid = ctk_grid_new ();
  ctk_grid_set_column_spacing (CTK_GRID (grid), 12);
  ctk_container_add (CTK_CONTAINER (self->priv->center_grid), grid);

  self->priv->title_label = ctk_label_new (NULL);
  ctk_label_set_ellipsize (CTK_LABEL (self->priv->title_label), PANGO_ELLIPSIZE_END);
  ctk_container_add (CTK_CONTAINER (grid), self->priv->title_label);

  self->priv->detail_label = ctk_label_new (NULL);
  ctk_label_set_ellipsize (CTK_LABEL (self->priv->detail_label), PANGO_ELLIPSIZE_END);
  ctk_widget_set_no_show_all (self->priv->detail_label, TRUE);
  ctk_style_context_add_class (ctk_widget_get_style_context (self->priv->detail_label), "dim-label");
  ctk_container_add (CTK_CONTAINER (grid), self->priv->detail_label);

  /* centered mode group */
  self->priv->modes_box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
  ctk_box_set_homogeneous (CTK_BOX (self->priv->modes_box), TRUE);
  ctk_widget_set_no_show_all (self->priv->modes_box, TRUE);
  ctk_style_context_add_class (ctk_widget_get_style_context (self->priv->modes_box), "linked");
  ctk_container_add (CTK_CONTAINER (self->priv->center_grid), self->priv->modes_box);

  /* right section */
  self->priv->right_group = ctk_tool_item_new ();
  ctk_widget_set_margin_start (CTK_WIDGET (self->priv->right_group), 12);
  ctk_toolbar_insert (tb, self->priv->right_group, -1);
  ctk_size_group_add_widget (self->priv->vertical_size_group,
                             CTK_WIDGET (self->priv->right_group));

  self->priv->right_grid = ctk_grid_new ();
  ctk_grid_set_column_spacing (CTK_GRID (self->priv->right_grid), 12);
  ctk_container_add (CTK_CONTAINER (self->priv->right_group), self->priv->right_grid);
  ctk_widget_set_halign (self->priv->right_grid, CTK_ALIGN_END);

  self->priv->size_group = ctk_size_group_new (CTK_SIZE_GROUP_HORIZONTAL);
  ctk_size_group_add_widget (self->priv->size_group, CTK_WIDGET (self->priv->left_group));
  ctk_size_group_add_widget (self->priv->size_group, CTK_WIDGET (self->priv->right_group));
}

static void
gd_main_toolbar_init (GdMainToolbar *self)
{
  self->priv = gd_main_toolbar_get_instance_private (self);
}

static void
gd_main_toolbar_class_init (GdMainToolbarClass *klass)
{
  GObjectClass *oclass;

  oclass = G_OBJECT_CLASS (klass);
  oclass->constructed = gd_main_toolbar_constructed;
  oclass->set_property = gd_main_toolbar_set_property;
  oclass->get_property = gd_main_toolbar_get_property;
  oclass->dispose = gd_main_toolbar_dispose;

  g_object_class_install_property (oclass,
                                   PROP_SHOW_MODES,
                                   g_param_spec_boolean ("show-modes",
                                                         "Show Modes",
                                                         "Show Modes",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

void
gd_main_toolbar_clear (GdMainToolbar *self)
{
  /* reset labels */
  ctk_label_set_text (CTK_LABEL (self->priv->title_label), "");
  ctk_label_set_text (CTK_LABEL (self->priv->detail_label), "");

  /* clear all added buttons */
  ctk_container_foreach (CTK_CONTAINER (self->priv->left_grid),
                         (CtkCallback) ctk_widget_destroy, self);
  ctk_container_foreach (CTK_CONTAINER (self->priv->modes_box),
                         (CtkCallback) ctk_widget_destroy, self);
  ctk_container_foreach (CTK_CONTAINER (self->priv->right_grid),
                         (CtkCallback) ctk_widget_destroy, self);
}

/**
 * gd_main_toolbar_set_labels:
 * @self:
 * @primary: (allow-none):
 * @detail: (allow-none):
 *
 */
void
gd_main_toolbar_set_labels (GdMainToolbar *self,
                            const gchar *primary,
                            const gchar *detail)
{
  gchar *real_primary = NULL;

  if (primary != NULL)
    real_primary = g_markup_printf_escaped ("<b>%s</b>", primary);

  if (real_primary == NULL)
    {
      ctk_label_set_markup (CTK_LABEL (self->priv->title_label), "");
      ctk_widget_hide (self->priv->title_label);
    }
  else
    {
      ctk_label_set_markup (CTK_LABEL (self->priv->title_label), real_primary);
      ctk_widget_show (self->priv->title_label);
    }

  if (detail == NULL)
    {
      ctk_label_set_text (CTK_LABEL (self->priv->detail_label), "");
      ctk_widget_hide (self->priv->detail_label);
    }
  else
    {
      ctk_label_set_text (CTK_LABEL (self->priv->detail_label), detail);
      ctk_widget_show (self->priv->detail_label);
    }

  g_free (real_primary);
}

CtkWidget *
gd_main_toolbar_new (void)
{
  return g_object_new (GD_TYPE_MAIN_TOOLBAR, NULL);
}

static CtkWidget *
add_button_internal (GdMainToolbar *self,
                     const gchar *icon_name,
                     const gchar *label,
                     gboolean pack_start,
                     ChildType type)
{
  CtkWidget *button;

  if (icon_name != NULL)
    {
      button = get_symbolic_button (icon_name, type);
      if (label != NULL)
        ctk_widget_set_tooltip_text (button, label);
    }
  else if (label != NULL)
    {
      button = get_text_button (label, type);
    }
  else
    {
      button = get_empty_button (type);
    }

  gd_main_toolbar_add_widget (self, button, pack_start);

  ctk_widget_show_all (button);

  return button;
}

/**
 * gd_main_toolbar_set_labels_menu:
 * @self:
 * @menu: (allow-none):
 *
 */
void
gd_main_toolbar_set_labels_menu (GdMainToolbar *self,
                                 GMenuModel    *menu)
{
  CtkWidget *button, *grid, *w;

  if (menu == NULL &&
      ((ctk_widget_get_parent (self->priv->labels_grid) == self->priv->center_grid) ||
       self->priv->center_menu_child == NULL))
    return;

  if (menu != NULL)
    {
      g_object_ref (self->priv->labels_grid);
      ctk_container_remove (CTK_CONTAINER (self->priv->center_grid),
                            self->priv->labels_grid);

      self->priv->center_menu_child = grid = ctk_grid_new ();
      ctk_grid_set_column_spacing (CTK_GRID (grid), 6);
      ctk_container_add (CTK_CONTAINER (grid), self->priv->labels_grid);
      g_object_unref (self->priv->labels_grid);

      w = ctk_image_new_from_icon_name ("pan-down-symbolic", CTK_ICON_SIZE_BUTTON);
      ctk_container_add (CTK_CONTAINER (grid), w);

      self->priv->center_menu = button = ctk_menu_button_new ();
      ctk_style_context_add_class (ctk_widget_get_style_context (self->priv->center_menu),
                                   "selection-menu");
      ctk_widget_destroy (ctk_bin_get_child (CTK_BIN (button)));
      ctk_widget_set_halign (button, CTK_ALIGN_CENTER);
      ctk_menu_button_set_menu_model (CTK_MENU_BUTTON (button), menu);
      ctk_container_add (CTK_CONTAINER (self->priv->center_menu), grid);

      ctk_container_add (CTK_CONTAINER (self->priv->center_grid), button);
    }
  else
    {
      g_object_ref (self->priv->labels_grid);
      ctk_container_remove (CTK_CONTAINER (self->priv->center_menu_child),
                            self->priv->labels_grid);
      ctk_widget_destroy (self->priv->center_menu);

      self->priv->center_menu = NULL;
      self->priv->center_menu_child = NULL;

      ctk_container_add (CTK_CONTAINER (self->priv->center_grid),
                         self->priv->labels_grid);
      g_object_unref (self->priv->labels_grid);
    }

  ctk_widget_show_all (self->priv->center_grid);
}

/**
 * gd_main_toolbar_add_mode:
 * @self:
 * @label:
 *
 * Returns: (transfer none):
 */
CtkWidget *
gd_main_toolbar_add_mode (GdMainToolbar *self,
                          const gchar *label)
{
  CtkWidget *button;
  GList *group;

  button = ctk_radio_button_new_with_label (NULL, label);
  ctk_toggle_button_set_mode (CTK_TOGGLE_BUTTON (button), FALSE);
  ctk_widget_set_size_request (button, 100, -1);
  ctk_style_context_add_class (ctk_widget_get_style_context (button), "raised");
  ctk_style_context_add_class (ctk_widget_get_style_context (button), "text-button");

  group = ctk_container_get_children (CTK_CONTAINER (self->priv->modes_box));
  if (group != NULL)
    {
      ctk_radio_button_join_group (CTK_RADIO_BUTTON (button), CTK_RADIO_BUTTON (group->data));
      g_list_free (group);
    }

  ctk_container_add (CTK_CONTAINER (self->priv->modes_box), button);
  ctk_widget_show (button);

  return button;
}

/**
 * gd_main_toolbar_add_button:
 * @self:
 * @icon_name: (allow-none):
 * @label: (allow-none):
 * @pack_start:
 *
 * Returns: (transfer none):
 */
CtkWidget *
gd_main_toolbar_add_button (GdMainToolbar *self,
                            const gchar *icon_name,
                            const gchar *label,
                            gboolean pack_start)
{
  return add_button_internal (self, icon_name, label, pack_start, CHILD_NORMAL);
}

/**
 * gd_main_toolbar_add_menu:
 * @self:
 * @icon_name: (allow-none):
 * @label: (allow-none):
 * @pack_start:
 *
 * Returns: (transfer none):
 */
CtkWidget *
gd_main_toolbar_add_menu (GdMainToolbar *self,
                          const gchar *icon_name,
                          const gchar *label,
                          gboolean pack_start)
{
  return add_button_internal (self, icon_name, label, pack_start, CHILD_MENU);
}

/**
 * gd_main_toolbar_add_toggle:
 * @self:
 * @icon_name: (allow-none):
 * @label: (allow-none):
 * @pack_start:
 *
 * Returns: (transfer none):
 */
CtkWidget *
gd_main_toolbar_add_toggle (GdMainToolbar *self,
                            const gchar *icon_name,
                            const gchar *label,
                            gboolean pack_start)
{
  return add_button_internal (self, icon_name, label, pack_start, CHILD_TOGGLE);
}

/**
 * gd_main_toolbar_add_widget:
 * @self:
 * @widget:
 * @pack_start:
 *
 */
void
gd_main_toolbar_add_widget (GdMainToolbar *self,
                            CtkWidget *widget,
                            gboolean pack_start)
{
  if (pack_start)
    ctk_container_add (CTK_CONTAINER (self->priv->left_grid), widget);
  else
    ctk_container_add (CTK_CONTAINER (self->priv->right_grid), widget);
}

