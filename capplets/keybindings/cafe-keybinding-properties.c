/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <ctk/ctk.h>
#include <gio/gio.h>
#include <cdk/cdkx.h>
#include <X11/Xatom.h>
#include <glib/gi18n.h>
#include <cdk/cdkkeysyms.h>

#include "wm-common.h"
#include "capplet-util.h"
#include "eggcellrendererkeys.h"
#include "activate-settings-daemon.h"
#include "dconf-util.h"

#define GSETTINGS_KEYBINDINGS_DIR "/org/cafe/desktop/keybindings/"
#define CUSTOM_KEYBINDING_SCHEMA "org.cafe.control-center.keybinding"

#define MAX_ELEMENTS_BEFORE_SCROLLING 10
#define MAX_CUSTOM_SHORTCUTS 1000
#define RESPONSE_ADD 0
#define RESPONSE_REMOVE 1

typedef struct {
  /* The untranslated name, combine with ->package to translate */
  char *name;
  /* The gettext package to use to translate the section title */
  char *package;
  /* Name of the window manager the keys would apply to */
  char *wm_name;
  /* The GSettings schema for the whole file */
  char *schema;
  /* an array of KeyListEntry */
  GArray *entries;
} KeyList;

typedef enum {
  COMPARISON_NONE = 0,
  COMPARISON_GT,
  COMPARISON_LT,
  COMPARISON_EQ
} Comparison;

typedef struct
{
  char *gsettings_path;
  char *schema;
  char *name;
  int value;
  char *value_schema; /* gsettings schema for key/value */
  char *value_key;
  char *description;
  char *description_key;
  char *cmd_key;
  Comparison comparison;
} KeyListEntry;

enum
{
  DESCRIPTION_COLUMN,
  KEYENTRY_COLUMN,
  N_COLUMNS
};

typedef struct
{
  GSettings *settings;
  char *gsettings_path;
  char *gsettings_key;
  guint keyval;
  guint keycode;
  EggVirtualModifierType mask;
  gboolean editable;
  CtkTreeModel *model;
  char *description;
  char *desc_gsettings_key;
  gboolean desc_editable;
  char *command;
  char *cmd_gsettings_key;
  gboolean cmd_editable;
  gulong gsettings_cnxn;
  gulong gsettings_cnxn_desc;
  gulong gsettings_cnxn_cmd;
} KeyEntry;

static gboolean block_accels = FALSE;
static CtkWidget *custom_shortcut_dialog = NULL;
static CtkWidget *custom_shortcut_name_entry = NULL;
static CtkWidget *custom_shortcut_command_entry = NULL;

static CtkWidget* _ctk_builder_get_widget(CtkBuilder* builder, const gchar* name)
{
    return CTK_WIDGET (ctk_builder_get_object (builder, name));
}

static CtkBuilder *
create_builder (void)
{
  CtkBuilder *builder = ctk_builder_new();
  GError *error = NULL;

  if (ctk_builder_add_from_resource (builder, "/org/cafe/ccc/keybindings/cafe-keybinding-properties.ui", &error) == 0) {
    g_warning ("Could not load UI: %s", error->message);
    g_error_free (error);
    g_object_unref (builder);
    builder = NULL;
  }

  return builder;
}

static char* binding_name(guint keyval, guint keycode, EggVirtualModifierType mask, gboolean translate)
{
    if (keyval != 0 || keycode != 0)
    {
        if (translate)
        {
            return egg_virtual_accelerator_label (keyval, keycode, mask);
        }
        else
        {
            return egg_virtual_accelerator_name (keyval, keycode, mask);
        }
    }
    else
    {
        return g_strdup (translate ? _("Disabled") : "");
    }
}

static gboolean
binding_from_string (const char             *str,
                     guint                  *accelerator_key,
             guint          *keycode,
                     EggVirtualModifierType *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);

  if (str == NULL || strcmp (str, "disabled") == 0)
    {
      *accelerator_key = 0;
      *keycode = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  egg_accelerator_parse_virtual (str, accelerator_key, keycode, accelerator_mods);

  if (*accelerator_key == 0)
    return FALSE;
  else
    return TRUE;
}

static void
accel_set_func (CtkTreeViewColumn *tree_column,
                CtkCellRenderer   *cell,
                CtkTreeModel      *model,
                CtkTreeIter       *iter,
                gpointer           data)
{
  KeyEntry *key_entry;

  ctk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key_entry,
                      -1);

  if (key_entry == NULL)
    g_object_set (cell,
          "visible", FALSE,
          NULL);
  else if (! key_entry->editable)
    g_object_set (cell,
          "visible", TRUE,
          "editable", FALSE,
          "accel_key", key_entry->keyval,
          "accel_mask", key_entry->mask,
          "keycode", key_entry->keycode,
          "style", PANGO_STYLE_ITALIC,
          NULL);
  else
    g_object_set (cell,
          "visible", TRUE,
          "editable", TRUE,
          "accel_key", key_entry->keyval,
          "accel_mask", key_entry->mask,
          "keycode", key_entry->keycode,
          "style", PANGO_STYLE_NORMAL,
          NULL);
}

static void
description_set_func (CtkTreeViewColumn *tree_column,
                      CtkCellRenderer   *cell,
                      CtkTreeModel      *model,
                      CtkTreeIter       *iter,
                      gpointer           data)
{
  KeyEntry *key_entry;

  ctk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key_entry,
                      -1);

  if (key_entry != NULL)
    g_object_set (cell,
          "editable", FALSE,
          "text", key_entry->description != NULL ?
              key_entry->description : _("<Unknown Action>"),
          NULL);
  else
    g_object_set (cell,
          "editable", FALSE, NULL);
}

static gboolean
keybinding_key_changed_foreach (CtkTreeModel *model,
                CtkTreePath  *path,
                CtkTreeIter  *iter,
                gpointer      user_data)
{
  KeyEntry *key_entry;
  KeyEntry *tmp_key_entry;

  key_entry = (KeyEntry *)user_data;
  ctk_tree_model_get (key_entry->model, iter,
              KEYENTRY_COLUMN, &tmp_key_entry,
              -1);

  if (key_entry == tmp_key_entry)
    {
      ctk_tree_model_row_changed (key_entry->model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
keybinding_key_changed (GSettings *settings,
                        gchar     *key,
                        KeyEntry  *key_entry)
{
  gchar *key_value;

  key_value = g_settings_get_string (settings, key);

  binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
  key_entry->editable = g_settings_is_writable (settings, key);

  /* update the model */
  ctk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
keybinding_description_changed (GSettings *settings,
                                gchar     *key,
                                KeyEntry  *key_entry)
{
  gchar *key_value;

  key_value = g_settings_get_string (settings, key);

  g_free (key_entry->description);
  key_entry->description = key_value ? g_strdup (key_value) : NULL;
  g_free (key_value);

  key_entry->desc_editable = g_settings_is_writable (settings, key);

  /* update the model */
  ctk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
keybinding_command_changed (GSettings *settings,
                            gchar     *key,
                            KeyEntry  *key_entry)
{
  gchar *key_value;

  key_value = g_settings_get_string (settings, key);

  g_free (key_entry->command);
  key_entry->command = key_value ? g_strdup (key_value) : NULL;
  key_entry->cmd_editable = g_settings_is_writable (settings, key);
  g_free (key_value);

  /* update the model */
  ctk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static int
keyentry_sort_func (CtkTreeModel *model,
                    CtkTreeIter  *a,
                    CtkTreeIter  *b,
                    gpointer      user_data)
{
  KeyEntry *key_entry_a;
  KeyEntry *key_entry_b;
  int retval;

  key_entry_a = NULL;
  ctk_tree_model_get (model, a,
                      KEYENTRY_COLUMN, &key_entry_a,
                      -1);

  key_entry_b = NULL;
  ctk_tree_model_get (model, b,
                      KEYENTRY_COLUMN, &key_entry_b,
                      -1);

  if (key_entry_a && key_entry_b)
    {
      if ((key_entry_a->keyval || key_entry_a->keycode) &&
          (key_entry_b->keyval || key_entry_b->keycode))
        {
          gchar *name_a, *name_b;

          name_a = binding_name (key_entry_a->keyval,
                                 key_entry_a->keycode,
                                 key_entry_a->mask,
                                 TRUE);

          name_b = binding_name (key_entry_b->keyval,
                                 key_entry_b->keycode,
                                 key_entry_b->mask,
                                 TRUE);

          retval = g_utf8_collate (name_a, name_b);

          g_free (name_a);
          g_free (name_b);
        }
      else if (key_entry_a->keyval || key_entry_a->keycode)
        retval = -1;
      else if (key_entry_b->keyval || key_entry_b->keycode)
        retval = 1;
      else
        retval = 0;
    }
  else if (key_entry_a)
    retval = -1;
  else if (key_entry_b)
    retval = 1;
  else
    retval = 0;

  return retval;
}

static void
clear_old_model (CtkBuilder *builder)
{
  CtkWidget *tree_view;
  CtkWidget *actions_swindow;
  CtkTreeModel *model;

  tree_view = _ctk_builder_get_widget (builder, "shortcut_treeview");
  model = ctk_tree_view_get_model (CTK_TREE_VIEW (tree_view));

  if (model == NULL)
    {
      /* create a new model */
      model = (CtkTreeModel *) ctk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

      ctk_tree_sortable_set_sort_func (CTK_TREE_SORTABLE (model),
                                       KEYENTRY_COLUMN,
                                       keyentry_sort_func,
                                       NULL, NULL);

      ctk_tree_view_set_model (CTK_TREE_VIEW (tree_view), model);

      g_object_unref (model);
    }
  else
    {
      /* clear the existing model */
      gboolean valid;
      CtkTreeIter iter;
      KeyEntry *key_entry;

      for (valid = ctk_tree_model_get_iter_first (model, &iter);
           valid;
           valid = ctk_tree_model_iter_next (model, &iter))
        {
          ctk_tree_model_get (model, &iter,
                              KEYENTRY_COLUMN, &key_entry,
                              -1);

          if (key_entry != NULL)
            {
              g_signal_handler_disconnect (key_entry->settings, key_entry->gsettings_cnxn);
              if (key_entry->gsettings_cnxn_desc != 0)
                g_signal_handler_disconnect (key_entry->settings, key_entry->gsettings_cnxn_desc);
              if (key_entry->gsettings_cnxn_cmd != 0)
                g_signal_handler_disconnect (key_entry->settings, key_entry->gsettings_cnxn_cmd);

              g_object_unref (key_entry->settings);
              if (key_entry->gsettings_path)
                g_free (key_entry->gsettings_path);
              g_free (key_entry->gsettings_key);
              g_free (key_entry->description);
              g_free (key_entry->desc_gsettings_key);
              g_free (key_entry->command);
              g_free (key_entry->cmd_gsettings_key);
              g_free (key_entry);
            }
        }

      ctk_tree_store_clear (CTK_TREE_STORE (model));
    }

  actions_swindow = _ctk_builder_get_widget (builder, "actions_swindow");
  ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (actions_swindow),
                  CTK_POLICY_NEVER, CTK_POLICY_NEVER);
  ctk_widget_set_size_request (actions_swindow, -1, -1);
}

typedef struct {
  const char *key;
  const char *path;
  const char *schema;
  gboolean found;
} KeyMatchData;

static gboolean key_match(CtkTreeModel* model, CtkTreePath* path, CtkTreeIter* iter, gpointer data)
{
    KeyMatchData* match_data = data;
    KeyEntry* element = NULL;
    gchar *element_schema = NULL;
    gchar *element_path = NULL;

    ctk_tree_model_get(model, iter,
        KEYENTRY_COLUMN, &element,
        -1);

    if (element && element->settings && G_IS_SETTINGS(element->settings))
    {
            g_object_get (element->settings, "schema-id", &element_schema, NULL);
        g_object_get (element->settings, "path", &element_path, NULL);
    }

    if (element && g_strcmp0(element->gsettings_key, match_data->key) == 0
            && g_strcmp0(element_schema, match_data->schema) == 0
            && g_strcmp0(element_path, match_data->path) == 0)
    {
        match_data->found = TRUE;
        return TRUE;
    }

    return FALSE;
}

static gboolean key_is_already_shown(CtkTreeModel* model, const KeyListEntry* entry)
{
    KeyMatchData data;

    data.key = entry->name;
    data.schema = entry->schema;
    data.path = entry->gsettings_path;
    data.found = FALSE;
    ctk_tree_model_foreach(model, key_match, &data);

    return data.found;
}

static gboolean should_show_key(const KeyListEntry* entry)
{
    GSettings *settings;
    int value;

    if (entry->comparison == COMPARISON_NONE)
    {
        return TRUE;
    }

    g_return_val_if_fail(entry->value_key != NULL, FALSE);
    g_return_val_if_fail(entry->value_schema != NULL, FALSE);

    settings = g_settings_new (entry->value_schema);
    value = g_settings_get_int (settings, entry->value_key);
    g_object_unref (settings);

    switch (entry->comparison)
    {
        case COMPARISON_NONE:
            /* For compiler warnings */
            g_assert_not_reached ();
            return FALSE;
        case COMPARISON_GT:
            if (value > entry->value)
            {
                return TRUE;
            }
            break;
        case COMPARISON_LT:
            if (value < entry->value)
            {
                return TRUE;
            }
            break;
        case COMPARISON_EQ:
            if (value == entry->value)
            {
                return TRUE;
            }
            break;
    }

    return FALSE;
}

static gboolean
count_rows_foreach (CtkTreeModel *model, CtkTreePath *path, CtkTreeIter *iter, gpointer data)
{
  gint *rows = data;

  (*rows)++;

  return FALSE;
}

static void
ensure_scrollbar (CtkBuilder *builder, int i)
{
  if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
    {
      CtkRequisition rectangle;
      GObject *actions_swindow = ctk_builder_get_object (builder,
                                                         "actions_swindow");
      CtkWidget *treeview = _ctk_builder_get_widget (builder,
                                                     "shortcut_treeview");
      ctk_widget_get_preferred_size (treeview, &rectangle, NULL);
      ctk_widget_set_size_request (treeview, -1, rectangle.height);
      ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (actions_swindow),
                      CTK_POLICY_NEVER, CTK_POLICY_AUTOMATIC);
    }
}

static void
find_section (CtkTreeModel *model,
              CtkTreeIter  *iter,
          const char   *title)
{
  gboolean success;

  success = ctk_tree_model_get_iter_first (model, iter);
  while (success)
    {
      char *description = NULL;

      ctk_tree_model_get (model, iter,
              DESCRIPTION_COLUMN, &description,
              -1);

      if (g_strcmp0 (description, title) == 0)
        return;
      success = ctk_tree_model_iter_next (model, iter);
    }

    ctk_tree_store_append (CTK_TREE_STORE (model), iter, NULL);
    ctk_tree_store_set (CTK_TREE_STORE (model), iter,
                        DESCRIPTION_COLUMN, title,
                        -1);
}

static void
append_keys_to_tree (CtkBuilder         *builder,
                     const gchar        *title,
                     const gchar        *schema,
                     const gchar        *package,
                     const KeyListEntry *keys_list)
{
  CtkTreeIter parent_iter, iter;
  CtkTreeModel *model;
  gint i, j;

  model = ctk_tree_view_get_model (CTK_TREE_VIEW (ctk_builder_get_object (builder, "shortcut_treeview")));

  /* Try to find a section parent iter, if it already exists */
  find_section (model, &iter, title);
  parent_iter = iter;

  i = 0;
  ctk_tree_model_foreach (model, count_rows_foreach, &i);

  /* If the header we just added is the MAX_ELEMENTS_BEFORE_SCROLLING th,
   * then we need to scroll now */
  ensure_scrollbar (builder, i - 1);

  for (j = 0; keys_list[j].name != NULL; j++)
    {
      GSettings *settings = NULL;
      gchar *settings_path;
      KeyEntry *key_entry;
      const gchar *key_string;
      gchar *key_value;
      gchar *description;
      gchar *command;

      if (!should_show_key (&keys_list[j]))
        continue;

      if (key_is_already_shown (model, &keys_list[j]))
        continue;

      key_string = keys_list[j].name;

      if (keys_list[j].gsettings_path != NULL)
        {
          settings = g_settings_new_with_path (schema, keys_list[j].gsettings_path);
          settings_path = g_strdup(keys_list[j].gsettings_path);
        }
      else
        {
          settings = g_settings_new (schema);
          settings_path = NULL;
        }

      if (keys_list[j].description_key != NULL)
        {
          /* it's a custom shortcut, so description is in gsettings */
          description = g_settings_get_string (settings, keys_list[j].description_key);
        }
      else
        {
          /* it's from keyfile, so description need to be translated */
          description = keys_list[j].description;
          if (package)
            {
              bind_textdomain_codeset (package, "UTF-8");
              description = dgettext (package, description);
            }
          else
            {
              description = _(description);
            }
        }

      if (description == NULL)
        {
          /* Only print a warning for keys that should have a schema */
          if (keys_list[j].description_key == NULL)
            g_warning ("No description for key '%s'", key_string);
        }



      if (keys_list[j].cmd_key != NULL)
        {
          command = g_settings_get_string (settings, keys_list[j].cmd_key);
        }
      else
        {
          command = NULL;
        }

      key_entry = g_new0 (KeyEntry, 1);
      key_entry->settings = settings;
      key_entry->gsettings_path = settings_path;
      key_entry->gsettings_key = g_strdup (key_string);
      key_entry->editable = g_settings_is_writable (settings, key_string);
      key_entry->model = model;
      key_entry->description = description;
      key_entry->command = command;
      if (keys_list[j].description_key != NULL)
        {
          key_entry->desc_gsettings_key =  g_strdup (keys_list[j].description_key);
          key_entry->desc_editable = g_settings_is_writable (settings, key_entry->desc_gsettings_key);
          gchar *gsettings_signal = g_strconcat ("changed::", key_entry->desc_gsettings_key, NULL);
          key_entry->gsettings_cnxn_desc = g_signal_connect (settings,
                                                             gsettings_signal,
                                                             G_CALLBACK (keybinding_description_changed),
                                                             key_entry);
          g_free (gsettings_signal);
        }
      if (keys_list[j].cmd_key != NULL)
        {
          key_entry->cmd_gsettings_key =  g_strdup (keys_list[j].cmd_key);
          key_entry->cmd_editable = g_settings_is_writable (settings, key_entry->cmd_gsettings_key);
          gchar *gsettings_signal = g_strconcat ("changed::", key_entry->cmd_gsettings_key, NULL);
          key_entry->gsettings_cnxn_cmd = g_signal_connect (settings,
                                                            gsettings_signal,
                                                            G_CALLBACK (keybinding_command_changed),
                                                            key_entry);
          g_free (gsettings_signal);
        }

      gchar *gsettings_signal = g_strconcat ("changed::", key_string, NULL);
      key_entry->gsettings_cnxn = g_signal_connect (settings,
                                                    gsettings_signal,
                                                    G_CALLBACK (keybinding_key_changed),
                                                    key_entry);
      g_free (gsettings_signal);

      key_value = g_settings_get_string (settings, key_string);
      binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
      g_free (key_value);

      ensure_scrollbar (builder, i);

      ++i;
      ctk_tree_store_append (CTK_TREE_STORE (model), &iter, &parent_iter);
      /* we use the DESCRIPTION_COLUMN only for the section headers */
      ctk_tree_store_set (CTK_TREE_STORE (model), &iter,
              KEYENTRY_COLUMN, key_entry,
              -1);
      ctk_tree_view_expand_all (CTK_TREE_VIEW (ctk_builder_get_object (builder, "shortcut_treeview")));
    }

  /* Don't show an empty section */
  if (ctk_tree_model_iter_n_children (model, &parent_iter) == 0)
    ctk_tree_store_remove (CTK_TREE_STORE (model), &parent_iter);

  if (i == 0)
      ctk_widget_hide (_ctk_builder_get_widget (builder, "shortcuts_vbox"));
  else
      ctk_widget_show (_ctk_builder_get_widget (builder, "shortcuts_vbox"));
}

static void
parse_start_tag (GMarkupParseContext *ctx,
         const gchar         *element_name,
         const gchar        **attr_names,
         const gchar        **attr_values,
         gpointer             user_data,
         GError             **error)
{
  KeyList *keylist = (KeyList *) user_data;
  KeyListEntry key;
  const char *name, *value_key, *description, *value_schema;
  int value;
  Comparison comparison;
  const char *schema = NULL;

  name = NULL;

  /* The top-level element, names the section in the tree */
  if (g_str_equal (element_name, "KeyListEntries"))
    {
      const char *wm_name = NULL;
      const char *package = NULL;

      while (*attr_names && *attr_values)
        {
          if (g_str_equal (*attr_names, "name"))
            {
              if (**attr_values)
                name = *attr_values;
            }
          else if (g_str_equal (*attr_names, "wm_name"))
            {
             if (**attr_values)
                wm_name = *attr_values;
            }
          else if (g_str_equal (*attr_names, "package"))
          {
            if (**attr_values)
                package = *attr_values;
          }
          else if (g_str_equal (*attr_names, "schema"))
          {
            if (**attr_values)
                schema = *attr_values;
          }
          ++attr_names;
          ++attr_values;
        }

      if (name)
        {
          if (keylist->name)
            g_warning ("Duplicate section name");
          g_free (keylist->name);
          keylist->name = g_strdup (name);
        }
      if (wm_name)
        {
          if (keylist->wm_name)
            g_warning ("Duplicate window manager name");
          g_free (keylist->wm_name);
          keylist->wm_name = g_strdup (wm_name);
        }
      if (package)
        {
          if (keylist->package)
            g_warning ("Duplicate gettext package name");
          g_free (keylist->package);
          keylist->package = g_strdup (package);
        }
      if (schema)
        {
          if (keylist->schema)
            g_warning ("Duplicate schema name");
          g_free (keylist->schema);
          keylist->schema = g_strdup (schema);
        }
      return;
    }

  if (!g_str_equal (element_name, "KeyListEntry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  value = 0;
  comparison = COMPARISON_NONE;
  value_key = NULL;
  value_schema = NULL;
  description = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "name"))
        {
      /* skip if empty */
      if (**attr_values)
        name = *attr_values;
    } else if (g_str_equal (*attr_names, "value")) {
      if (**attr_values) {
        value = (int) g_ascii_strtoull (*attr_values, NULL, 0);
      }
    } else if (g_str_equal (*attr_names, "key")) {
      if (**attr_values) {
        value_key = *attr_values;
      }
    } else if (g_str_equal (*attr_names, "comparison")) {
      if (**attr_values) {
        if (g_str_equal (*attr_values, "gt")) {
          comparison = COMPARISON_GT;
        } else if (g_str_equal (*attr_values, "lt")) {
          comparison = COMPARISON_LT;
        } else if (g_str_equal (*attr_values, "eq")) {
          comparison = COMPARISON_EQ;
        }
      }
    } else if (g_str_equal (*attr_names, "description")) {
      if (**attr_values) {
        description = *attr_values;
      }
    } else if (g_str_equal (*attr_names, "schema")) {
      if (**attr_values) {
        value_schema = *attr_values;
      }
    }

      ++attr_names;
      ++attr_values;
    }

  if (name == NULL)
    return;

  key.name = g_strdup (name);
  key.gsettings_path = NULL;
  key.description_key = NULL;
  key.description = g_strdup(description);
  key.schema = g_strdup(schema);
  key.value = value;
  if (value_key) {
    key.value_key = g_strdup (value_key);
    key.value_schema = g_strdup (value_schema);
  }
  else {
    key.value_key = NULL;
    key.value_schema = NULL;
  }
  key.comparison = comparison;
  key.cmd_key = NULL;
  g_array_append_val (keylist->entries, key);
}

static gboolean
strv_contains (char **strv,
           char  *str)
{
  char **p = strv;
  for (p = strv; *p; p++)
    if (strcmp (*p, str) == 0)
      return TRUE;

  return FALSE;
}

static void
append_keys_to_tree_from_file (CtkBuilder *builder,
                   const char *filename,
                   char      **wm_keybindings)
{
  GMarkupParseContext *ctx;
  GMarkupParser parser = { parse_start_tag, NULL, NULL, NULL, NULL };
  KeyList *keylist;
  KeyListEntry key, *keys;
  GError *err = NULL;
  char *buf;
  const char *title;
  gsize buf_len;
  guint i;

  if (!g_file_get_contents (filename, &buf, &buf_len, &err))
    return;

  keylist = g_new0 (KeyList, 1);
  keylist->entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));
  ctx = g_markup_parse_context_new (&parser, 0, keylist, NULL);

  if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
    {
      g_warning ("Failed to parse '%s': '%s'", filename, err->message);
      g_error_free (err);
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_free (keylist->schema);
      for (i = 0; i < keylist->entries->len; i++)
        g_free (((KeyListEntry *) &(keylist->entries->data[i]))->name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      keylist = NULL;
    }
  g_markup_parse_context_free (ctx);
  g_free (buf);

  if (keylist == NULL)
    return;

  /* If there's no keys to add, or the settings apply to a window manager
   * that's not the one we're running */
  if (keylist->entries->len == 0
      || (keylist->wm_name != NULL && !strv_contains (wm_keybindings, keylist->wm_name))
      || keylist->name == NULL)
    {
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_free (keylist->schema);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return;
    }

  /* Empty KeyListEntry to end the array */
  key.name = NULL;
  key.description_key = NULL;
  key.value_key = NULL;
  key.value = 0;
  key.comparison = COMPARISON_NONE;
  g_array_append_val (keylist->entries, key);

  keys = (KeyListEntry *) g_array_free (keylist->entries, FALSE);
  if (keylist->package)
    {
      bind_textdomain_codeset (keylist->package, "UTF-8");
      title = dgettext (keylist->package, keylist->name);
    } else {
      title = _(keylist->name);
    }

  append_keys_to_tree (builder, title, keylist->schema, keylist->package, keys);

  g_free (keylist->name);
  g_free (keylist->package);
  for (i = 0; keys[i].name != NULL; i++)
    g_free (keys[i].name);
  g_free (keylist);
}

static void
append_keys_to_tree_from_gsettings (CtkBuilder *builder, const gchar *gsettings_path)
{
  gchar **custom_list;
  GArray *entries;
  KeyListEntry key;
  guint i;

  /* load custom shortcuts from GSettings */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  key.value_key = NULL;
  key.value = 0;
  key.comparison = COMPARISON_NONE;

  custom_list = dconf_util_list_subdirs (gsettings_path, FALSE);

  if (custom_list != NULL)
    {
      for (i = 0; custom_list[i] != NULL; i++)
        {
          key.gsettings_path = g_strdup_printf("%s%s", gsettings_path, custom_list[i]);
          key.name = g_strdup("binding");
          key.cmd_key = g_strdup("action");
          key.description_key = g_strdup("name");
          key.schema = NULL;
          g_array_append_val (entries, key);
        }
    }

  g_strfreev (custom_list);

  if (entries->len > 0)
    {
      KeyListEntry *keys;

      /* Empty KeyListEntry to end the array */
      key.gsettings_path = NULL;
      key.name = NULL;
      key.description_key = NULL;
      key.cmd_key = NULL;
      g_array_append_val (entries, key);

      keys = (KeyListEntry *) entries->data;
      append_keys_to_tree (builder, _("Custom Shortcuts"), CUSTOM_KEYBINDING_SCHEMA, NULL, keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
          g_free (keys[i].description_key);
          g_free (keys[i].cmd_key);
          g_free (keys[i].gsettings_path);
        }
    }

  g_array_free (entries, TRUE);
}

static void
reload_key_entries (CtkBuilder *builder)
{
  gchar **wm_keybindings;
  GList *list, *l;
  const gchar * const * data_dirs;
  GHashTable *loaded_files;
  guint i;

  wm_keybindings = wm_common_get_current_keybindings();

  clear_old_model (builder);

  loaded_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  data_dirs = g_get_system_data_dirs ();
  for (i = 0; data_dirs[i] != NULL; i++)
    {
      g_autofree gchar *dir_path = NULL;
      GDir *dir;
      const gchar *name;

      dir_path = g_build_filename (data_dirs[i], "cafe-control-center", "keybindings", NULL);
      g_debug ("Keybinding dir: %s", dir_path);

      dir = g_dir_open (dir_path, 0, NULL);
      if (!dir)
        continue;

      for (name = g_dir_read_name (dir) ; name ; name = g_dir_read_name (dir))
        {
          if (g_str_has_suffix (name, ".xml") == FALSE)
            continue;

          if (g_hash_table_lookup (loaded_files, name) != NULL)
            {
              g_debug ("Not loading %s, it was already loaded from another directory", name);
              continue;
            }

          g_hash_table_insert (loaded_files, g_strdup (name), g_strdup (dir_path));
        }

      g_dir_close (dir);
    }
  list = g_hash_table_get_keys (loaded_files);
  list = g_list_sort(list, (GCompareFunc) g_str_equal);
  for (l = list; l != NULL; l = l->next)
    {
      g_autofree gchar *path = NULL;
      path = g_build_filename (g_hash_table_lookup (loaded_files, l->data), l->data, NULL);
      g_debug ("Keybinding file: %s", path);
      append_keys_to_tree_from_file (builder, path, wm_keybindings);
    }
  g_list_free (list);
  g_hash_table_destroy (loaded_files);

  /* Load custom shortcuts _after_ system-provided ones,
   * since some of the custom shortcuts may also be listed
   * in a file. Loading the custom shortcuts last makes
   * such keys not show up in the custom section.
   */
  append_keys_to_tree_from_gsettings (builder, GSETTINGS_KEYBINDINGS_DIR);

  g_strfreev (wm_keybindings);
}

static void
key_entry_controlling_key_changed (GSettings *settings, gchar *key, gpointer user_data)
{
  reload_key_entries (user_data);
}

static gboolean cb_check_for_uniqueness(CtkTreeModel* model, CtkTreePath* path, CtkTreeIter* iter, KeyEntry* new_key)
{
    KeyEntry* element;

    ctk_tree_model_get (new_key->model, iter,
        KEYENTRY_COLUMN, &element,
        -1);

    /* no conflict for : blanks, different modifiers, or ourselves */
    if (element == NULL || new_key->mask != element->mask)
    {
        return FALSE;
    }

    gchar *new_key_schema = NULL;
    gchar *element_schema = NULL;
    gchar *new_key_path = NULL;
    gchar *element_path = NULL;

    if (new_key && new_key->settings)
    {
            g_object_get (new_key->settings, "schema-id", &new_key_schema, NULL);
        g_object_get (new_key->settings, "path", &new_key_path, NULL);
    }

    if (element->settings)
    {
            g_object_get (element->settings, "schema-id", &element_schema, NULL);
        g_object_get (element->settings, "path", &element_path, NULL);
    }

    if (!g_strcmp0 (new_key->gsettings_key, element->gsettings_key) &&
        !g_strcmp0 (new_key_schema, element_schema) &&
        !g_strcmp0 (new_key_path, element_path))
    {
        return FALSE;
    }

    if (new_key->keyval != 0)
    {
        if (new_key->keyval != element->keyval)
        {
            return FALSE;
        }
    }
    else if (element->keyval != 0 || new_key->keycode != element->keycode)
    {
        return FALSE;
    }

    new_key->editable = FALSE;
    new_key->settings = element->settings;
    new_key->gsettings_key = element->gsettings_key;
    new_key->description = element->description;
    new_key->desc_gsettings_key = element->desc_gsettings_key;
    new_key->desc_editable = element->desc_editable;

    return TRUE;
}

static const guint forbidden_keyvals[] = {
    /* Navigation keys */
    CDK_KEY_Home,
    CDK_KEY_Left,
    CDK_KEY_Up,
    CDK_KEY_Right,
    CDK_KEY_Down,
    CDK_KEY_Page_Up,
    CDK_KEY_Page_Down,
    CDK_KEY_End,
    CDK_KEY_Tab,

    /* Return */
    CDK_KEY_KP_Enter,
    CDK_KEY_Return,

    CDK_KEY_space,
    CDK_KEY_Mode_switch
};

static gboolean keyval_is_forbidden(guint keyval)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(forbidden_keyvals); i++)
    {
        if (keyval == forbidden_keyvals[i])
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void show_error(CtkWindow* parent, GError* err)
{
  CtkWidget *dialog;

  dialog = ctk_message_dialog_new (parent,
                   CTK_DIALOG_DESTROY_WITH_PARENT | CTK_DIALOG_MODAL,
                   CTK_MESSAGE_WARNING,
                   CTK_BUTTONS_OK,
                   _("Error saving the new shortcut"));

  ctk_message_dialog_format_secondary_text (CTK_MESSAGE_DIALOG (dialog),
                                            "%s", err->message);
  ctk_dialog_run (CTK_DIALOG (dialog));
  ctk_widget_destroy (dialog);
}

static void accel_edited_callback(CtkCellRendererText* cell, const char* path_string, guint keyval, EggVirtualModifierType mask, guint keycode, gpointer data)
{
    CtkTreeView* view = (CtkTreeView*) data;
    CtkTreeModel* model;
    CtkTreePath* path = ctk_tree_path_new_from_string (path_string);
    CtkTreeIter iter;
    KeyEntry* key_entry, tmp_key;
    char* str;

    block_accels = FALSE;

    model = ctk_tree_view_get_model (view);
    ctk_tree_model_get_iter (model, &iter, path);
    ctk_tree_path_free (path);
    ctk_tree_model_get (model, &iter,
                        KEYENTRY_COLUMN, &key_entry,
                        -1);

    /* sanity check */
    if (key_entry == NULL)
    {
        return;
    }

    /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
    mask &= ~EGG_VIRTUAL_LOCK_MASK;

    tmp_key.model  = model;
    tmp_key.keyval = keyval;
    tmp_key.keycode = keycode;
    tmp_key.mask   = mask;
    tmp_key.settings = key_entry->settings;
    tmp_key.gsettings_key = key_entry->gsettings_key;
    tmp_key.description = NULL;
    tmp_key.editable = TRUE; /* kludge to stuff in a return flag */

    if (keyval != 0 || keycode != 0) /* any number of keys can be disabled */
    {
        ctk_tree_model_foreach(model, (CtkTreeModelForeachFunc) cb_check_for_uniqueness, &tmp_key);
    }

    /* Check for unmodified keys */
    if (tmp_key.mask == 0 && tmp_key.keycode != 0)
    {
        if ((tmp_key.keyval >= CDK_KEY_a && tmp_key.keyval <= CDK_KEY_z)
            || (tmp_key.keyval >= CDK_KEY_A && tmp_key.keyval <= CDK_KEY_Z)
            || (tmp_key.keyval >= CDK_KEY_0 && tmp_key.keyval <= CDK_KEY_9)
            || (tmp_key.keyval >= CDK_KEY_kana_fullstop && tmp_key.keyval <= CDK_KEY_semivoicedsound)
            || (tmp_key.keyval >= CDK_KEY_Arabic_comma && tmp_key.keyval <= CDK_KEY_Arabic_sukun)
            || (tmp_key.keyval >= CDK_KEY_Serbian_dje && tmp_key.keyval <= CDK_KEY_Cyrillic_HARDSIGN)
            || (tmp_key.keyval >= CDK_KEY_Greek_ALPHAaccent && tmp_key.keyval <= CDK_KEY_Greek_omega)
            || (tmp_key.keyval >= CDK_KEY_hebrew_doublelowline && tmp_key.keyval <= CDK_KEY_hebrew_taf)
            || (tmp_key.keyval >= CDK_KEY_Thai_kokai && tmp_key.keyval <= CDK_KEY_Thai_lekkao)
            || (tmp_key.keyval >= CDK_KEY_Hangul && tmp_key.keyval <= CDK_KEY_Hangul_Special)
            || (tmp_key.keyval >= CDK_KEY_Hangul_Kiyeog && tmp_key.keyval <= CDK_KEY_Hangul_J_YeorinHieuh)
            || keyval_is_forbidden (tmp_key.keyval))
        {

            CtkWidget *dialog;
            char *name;

            name = binding_name (keyval, keycode, mask, TRUE);

            dialog = ctk_message_dialog_new (
                CTK_WINDOW (ctk_widget_get_toplevel (CTK_WIDGET (view))),
                CTK_DIALOG_DESTROY_WITH_PARENT | CTK_DIALOG_MODAL,
                CTK_MESSAGE_WARNING,
                CTK_BUTTONS_CANCEL,
                _("The shortcut \"%s\" cannot be used because it will become impossible to type using this key.\n"
                "Please try with a key such as Control, Alt or Shift at the same time."),
                name);

            g_free (name);
            ctk_dialog_run (CTK_DIALOG (dialog));
            ctk_widget_destroy (dialog);

            /* set it back to its previous value. */
            egg_cell_renderer_keys_set_accelerator(
                EGG_CELL_RENDERER_KEYS(cell),
                key_entry->keyval,
                key_entry->keycode,
                key_entry->mask);
            return;
        }
    }

    /* flag to see if the new accelerator was in use by something */
    if (!tmp_key.editable)
    {
        CtkWidget* dialog;
        char* name;
        int response;

        name = binding_name(keyval, keycode, mask, TRUE);

        dialog = ctk_message_dialog_new(
            CTK_WINDOW(ctk_widget_get_toplevel(CTK_WIDGET(view))),
            CTK_DIALOG_DESTROY_WITH_PARENT | CTK_DIALOG_MODAL,
            CTK_MESSAGE_WARNING,
            CTK_BUTTONS_CANCEL,
            _("The shortcut \"%s\" is already used for\n\"%s\""),
            name,
            tmp_key.description ? tmp_key.description : tmp_key.gsettings_key);
            g_free (name);

        ctk_message_dialog_format_secondary_text (
            CTK_MESSAGE_DIALOG (dialog),
            _("If you reassign the shortcut to \"%s\", the \"%s\" shortcut "
            "will be disabled."),
            key_entry->description ? key_entry->description : key_entry->gsettings_key,
            tmp_key.description ? tmp_key.description : tmp_key.gsettings_key);

        ctk_dialog_add_button(CTK_DIALOG (dialog), _("_Reassign"), CTK_RESPONSE_ACCEPT);

        ctk_dialog_set_default_response(CTK_DIALOG (dialog), CTK_RESPONSE_ACCEPT);

        response = ctk_dialog_run (CTK_DIALOG (dialog));
        ctk_widget_destroy (dialog);

        if (response == CTK_RESPONSE_ACCEPT)
        {
            g_settings_set_string (tmp_key.settings, tmp_key.gsettings_key, "disabled");

            str = binding_name (keyval, keycode, mask, FALSE);
            g_settings_set_string (key_entry->settings, key_entry->gsettings_key, str);

            g_free (str);
        }
        else
        {
            /* set it back to its previous value. */
            egg_cell_renderer_keys_set_accelerator(
                EGG_CELL_RENDERER_KEYS(cell),
                key_entry->keyval,
                key_entry->keycode,
                key_entry->mask);
        }

        return;
    }

    str = binding_name (keyval, keycode, mask, FALSE);

    g_settings_set_string(
        key_entry->settings,
        key_entry->gsettings_key,
        str);

    g_free (str);

}

static void
accel_cleared_callback (CtkCellRendererText *cell,
            const char          *path_string,
            gpointer             data)
{
  CtkTreeView *view = (CtkTreeView *) data;
  CtkTreePath *path = ctk_tree_path_new_from_string (path_string);
  KeyEntry *key_entry;
  CtkTreeIter iter;
  CtkTreeModel *model;

  block_accels = FALSE;

  model = ctk_tree_view_get_model (view);
  ctk_tree_model_get_iter (model, &iter, path);
  ctk_tree_path_free (path);
  ctk_tree_model_get (model, &iter,
              KEYENTRY_COLUMN, &key_entry,
              -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  /* Unset the key */
  g_settings_set_string (key_entry->settings,
                         key_entry->gsettings_key,
                         "disabled");
}

static void
description_edited_callback (CtkCellRendererText *renderer,
                             gchar               *path_string,
                             gchar               *new_text,
                             gpointer             user_data)
{
  CtkTreeView *view = CTK_TREE_VIEW (user_data);
  CtkTreeModel *model;
  CtkTreePath *path = ctk_tree_path_new_from_string (path_string);
  CtkTreeIter iter;
  KeyEntry *key_entry;

  model = ctk_tree_view_get_model (view);
  ctk_tree_model_get_iter (model, &iter, path);
  ctk_tree_path_free (path);

  ctk_tree_model_get (model, &iter,
              KEYENTRY_COLUMN, &key_entry,
              -1);

  /* sanity check */
  if (key_entry == NULL || key_entry->desc_gsettings_key == NULL)
    return;

  if (!g_settings_set_string (key_entry->settings, key_entry->desc_gsettings_key, new_text))
    key_entry->desc_editable = FALSE;
}


typedef struct
{
  CtkTreeView *tree_view;
  CtkTreePath *path;
  CtkTreeViewColumn *column;
} IdleData;

static gboolean
real_start_editing_cb (IdleData *idle_data)
{
  ctk_widget_grab_focus (CTK_WIDGET (idle_data->tree_view));
  ctk_tree_view_set_cursor (idle_data->tree_view,
                idle_data->path,
                idle_data->column,
                TRUE);
  ctk_tree_path_free (idle_data->path);
  g_free (idle_data);
  return FALSE;
}

static gboolean
edit_custom_shortcut (KeyEntry *key)
{
  gint result;
  const gchar *text;
  gboolean ret;

  ctk_entry_set_text (CTK_ENTRY (custom_shortcut_name_entry), key->description ? key->description : "");
  ctk_widget_set_sensitive (custom_shortcut_name_entry, key->desc_editable);
  ctk_widget_grab_focus (custom_shortcut_name_entry);
  ctk_entry_set_text (CTK_ENTRY (custom_shortcut_command_entry), key->command ? key->command : "");
  ctk_widget_set_sensitive (custom_shortcut_command_entry, key->cmd_editable);

  ctk_window_present (CTK_WINDOW (custom_shortcut_dialog));
  result = ctk_dialog_run (CTK_DIALOG (custom_shortcut_dialog));
  switch (result)
    {
    case CTK_RESPONSE_OK:
      text = ctk_entry_get_text (CTK_ENTRY (custom_shortcut_name_entry));
      g_free (key->description);
      key->description = g_strdup (text);
      text = ctk_entry_get_text (CTK_ENTRY (custom_shortcut_command_entry));
      g_free (key->command);
      key->command = g_strdup (text);
      ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
    }

  ctk_widget_hide (custom_shortcut_dialog);

  return ret;
}

static gboolean
remove_custom_shortcut (CtkTreeModel *model, CtkTreeIter *iter)
{
  CtkTreeIter parent;
  KeyEntry *key;

  ctk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key,
                      -1);

  /* not a custom shortcut */
  if (key->command == NULL)
    return FALSE;

  g_signal_handler_disconnect (key->settings, key->gsettings_cnxn);
  if (key->gsettings_cnxn_desc != 0)
    g_signal_handler_disconnect (key->settings, key->gsettings_cnxn_desc);
  if (key->gsettings_cnxn_cmd != 0)
    g_signal_handler_disconnect (key->settings, key->gsettings_cnxn_cmd);

  dconf_util_recursive_reset (key->gsettings_path, NULL);
  g_object_unref (key->settings);

  g_free (key->gsettings_path);
  g_free (key->gsettings_key);
  g_free (key->description);
  g_free (key->desc_gsettings_key);
  g_free (key->command);
  g_free (key->cmd_gsettings_key);
  g_free (key);

  ctk_tree_model_iter_parent (model, &parent, iter);
  ctk_tree_store_remove (CTK_TREE_STORE (model), iter);
  if (!ctk_tree_model_iter_has_child (model, &parent))
    ctk_tree_store_remove (CTK_TREE_STORE (model), &parent);

  return TRUE;
}

static void
update_custom_shortcut (CtkTreeModel *model, CtkTreeIter *iter)
{
  KeyEntry *key;

  ctk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key,
                      -1);

  edit_custom_shortcut (key);
  if (key->command == NULL || key->command[0] == '\0')
    {
      remove_custom_shortcut (model, iter);
    }
  else
    {
      ctk_tree_store_set (CTK_TREE_STORE (model), iter,
              KEYENTRY_COLUMN, key, -1);
      if (key->description != NULL)
        g_settings_set_string (key->settings, key->desc_gsettings_key, key->description);
      else
        g_settings_reset (key->settings, key->desc_gsettings_key);
      g_settings_set_string (key->settings, key->cmd_gsettings_key, key->command);
    }
}

static gchar *
find_free_gsettings_path (GError **error)
{
  gchar **existing_dirs;
  gchar *dir = NULL;
  gchar *fulldir = NULL;
  int i;
  int j;
  gboolean found;

  existing_dirs = dconf_util_list_subdirs (GSETTINGS_KEYBINDINGS_DIR, FALSE);

  for (i = 0; i < MAX_CUSTOM_SHORTCUTS; i++)
    {
      found = TRUE;
      dir = g_strdup_printf ("custom%d/", i);
      for (j = 0; existing_dirs[j] != NULL; j++)
        if (!g_strcmp0(dir, existing_dirs[j]))
          {
            found = FALSE;
            g_free (dir);
            break;
          }
      if (found)
       break;
    }
    g_strfreev (existing_dirs);

  if (i == MAX_CUSTOM_SHORTCUTS)
    {
      g_free (dir);
      dir = NULL;
      g_set_error_literal (error,
                           g_quark_from_string ("Keyboard Shortcuts"),
                           0,
                           _("Too many custom shortcuts"));
    }

  fulldir = g_strdup_printf ("%s%s", GSETTINGS_KEYBINDINGS_DIR, dir);
  g_free (dir);
  return fulldir;
}

static void
add_custom_shortcut (CtkTreeView  *tree_view,
                     CtkTreeModel *model)
{
  KeyEntry *key_entry;
  CtkTreeIter iter;
  CtkTreeIter parent_iter;
  CtkTreePath *path;
  gchar *dir;
  GError *error;

  error = NULL;
  dir = find_free_gsettings_path (&error);
  if (dir == NULL)
    {
      show_error (CTK_WINDOW (ctk_widget_get_toplevel (CTK_WIDGET (tree_view))), error);

      g_error_free (error);
      return;
    }

  key_entry = g_new0 (KeyEntry, 1);
  key_entry->gsettings_path = g_strdup(dir);
  key_entry->gsettings_key = g_strdup("binding");
  key_entry->editable = TRUE;
  key_entry->model = model;
  key_entry->desc_gsettings_key = g_strdup("name");
  key_entry->description = g_strdup ("");
  key_entry->desc_editable = TRUE;
  key_entry->cmd_gsettings_key = g_strdup("action");
  key_entry->command = g_strdup ("");
  key_entry->cmd_editable = TRUE;
  g_free (dir);

  if (edit_custom_shortcut (key_entry) &&
      key_entry->command && key_entry->command[0])
    {
      find_section (model, &iter, _("Custom Shortcuts"));
      parent_iter = iter;
      ctk_tree_store_append (CTK_TREE_STORE (model), &iter, &parent_iter);
      ctk_tree_store_set (CTK_TREE_STORE (model), &iter, KEYENTRY_COLUMN, key_entry, -1);

      /* store in gsettings */
      key_entry->settings = g_settings_new_with_path (CUSTOM_KEYBINDING_SCHEMA, key_entry->gsettings_path);
      g_settings_set_string (key_entry->settings, key_entry->gsettings_key, "disabled");
      g_settings_set_string (key_entry->settings, key_entry->desc_gsettings_key, key_entry->description);
      g_settings_set_string (key_entry->settings, key_entry->cmd_gsettings_key, key_entry->command);

      /* add gsettings watches */
      key_entry->gsettings_cnxn_desc = g_signal_connect (key_entry->settings,
                                                         "changed::name",
                                                         G_CALLBACK (keybinding_description_changed),
                                                         key_entry);
      key_entry->gsettings_cnxn_cmd = g_signal_connect (key_entry->settings,
                                                        "changed::action",
                                                        G_CALLBACK (keybinding_command_changed),
                                                        key_entry);
      key_entry->gsettings_cnxn = g_signal_connect (key_entry->settings,
                                                    "changed::binding",
                                                    G_CALLBACK (keybinding_key_changed),
                                                    key_entry);

      /* make the new shortcut visible */
      path = ctk_tree_model_get_path (model, &iter);
      ctk_tree_view_expand_to_path (tree_view, path);
      ctk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0, 0);
      ctk_tree_path_free (path);
    }
  else
    {
      g_free (key_entry->gsettings_path);
      g_free (key_entry->gsettings_key);
      g_free (key_entry->description);
      g_free (key_entry->desc_gsettings_key);
      g_free (key_entry->command);
      g_free (key_entry->cmd_gsettings_key);
      g_free (key_entry);
    }
}

static void
start_editing_kb_cb (CtkTreeView *treeview,
              CtkTreePath *path,
              CtkTreeViewColumn *column,
              gpointer user_data)
{
      CtkTreeModel *model;
      CtkTreeIter iter;
      KeyEntry *key;

      model = ctk_tree_view_get_model (treeview);
      ctk_tree_model_get_iter (model, &iter, path);
      ctk_tree_model_get (model, &iter,
                          KEYENTRY_COLUMN, &key,
                         -1);

      if (key == NULL)
        {
      /* This is a section heading - expand or collapse */
      if (ctk_tree_view_row_expanded (treeview, path))
        ctk_tree_view_collapse_row (treeview, path);
      else
        ctk_tree_view_expand_row (treeview, path, FALSE);
          return;
    }

      /* if only the accel can be edited on the selected row
       * always select the accel column */
      if (key->desc_editable &&
          column == ctk_tree_view_get_column (treeview, 0))
        {
          ctk_widget_grab_focus (CTK_WIDGET (treeview));
          ctk_tree_view_set_cursor (treeview, path,
                                    ctk_tree_view_get_column (treeview, 0),
                                    FALSE);
          update_custom_shortcut (model, &iter);
        }
      else
        {
          ctk_widget_grab_focus (CTK_WIDGET (treeview));
          ctk_tree_view_set_cursor (treeview,
                                    path,
                                    ctk_tree_view_get_column (treeview, 1),
                                    TRUE);
        }
}

static gboolean
start_editing_cb (CtkTreeView    *tree_view,
          CdkEventButton *event,
          gpointer        user_data)
{
  CtkTreePath *path;
  CtkTreeViewColumn *column;

  if ((event->window != ctk_tree_view_get_bin_window (tree_view)) ||
      (event->type != CDK_2BUTTON_PRESS))
    return FALSE;

  if (ctk_tree_view_get_path_at_pos (tree_view,
                     (gint) event->x,
                     (gint) event->y,
                     &path, &column,
                     NULL, NULL))
    {
      IdleData *idle_data;
      CtkTreeModel *model;
      CtkTreeIter iter;
      KeyEntry *key;

      if (ctk_tree_path_get_depth (path) == 1)
    {
      ctk_tree_path_free (path);
      return FALSE;
    }

      model = ctk_tree_view_get_model (tree_view);
      ctk_tree_model_get_iter (model, &iter, path);
      ctk_tree_model_get (model, &iter,
                          KEYENTRY_COLUMN, &key,
                         -1);

      /* if only the accel can be edited on the selected row
       * always select the accel column */
      if (key->desc_editable &&
          column == ctk_tree_view_get_column (tree_view, 0))
        {
          ctk_widget_grab_focus (CTK_WIDGET (tree_view));
          ctk_tree_view_set_cursor (tree_view, path,
                                    ctk_tree_view_get_column (tree_view, 0),
                                    FALSE);
          update_custom_shortcut (model, &iter);
        }
      else
        {
          idle_data = g_new (IdleData, 1);
          idle_data->tree_view = tree_view;
          idle_data->path = path;
          idle_data->column = key->desc_editable ? column :
                              ctk_tree_view_get_column (tree_view, 1);
          g_idle_add ((GSourceFunc) real_start_editing_cb, idle_data);
          block_accels = TRUE;
        }
      g_signal_stop_emission_by_name (tree_view, "button_press_event");
    }
  return TRUE;
}

/* this handler is used to keep accels from activating while the user
 * is assigning a new shortcut so that he won't accidentally trigger one
 * of the widgets */
static gboolean maybe_block_accels(CtkWidget* widget, CdkEventKey* event, gpointer user_data)
{
    if (block_accels)
    {
        return ctk_window_propagate_key_event(CTK_WINDOW(widget), event);
    }

    return FALSE;
}

static void
cb_dialog_response (CtkWidget *widget, gint response_id, gpointer data)
{
  CtkBuilder *builder = data;
  CtkTreeView *treeview;
  CtkTreeModel *model;
  CtkTreeSelection *selection;
  CtkTreeIter iter;

  treeview = CTK_TREE_VIEW (ctk_builder_get_object (builder,
                                                    "shortcut_treeview"));
  model = ctk_tree_view_get_model (treeview);

  if (response_id == CTK_RESPONSE_HELP)
    {
      capplet_help (CTK_WINDOW (widget),
                    "goscustdesk-39");
    }
  else if (response_id == RESPONSE_ADD)
    {
      add_custom_shortcut (treeview, model);
    }
  else if (response_id == RESPONSE_REMOVE)
    {
      selection = ctk_tree_view_get_selection (treeview);
      if (ctk_tree_selection_get_selected (selection, NULL, &iter))
        {
          remove_custom_shortcut (model, &iter);
        }
    }
  else
    {
      clear_old_model (builder);
      ctk_main_quit ();
    }
}

static void
selection_changed (CtkTreeSelection *selection, gpointer data)
{
  CtkWidget *button = data;
  CtkTreeModel *model;
  CtkTreeIter iter;
  KeyEntry *key;
  gboolean can_remove;

  can_remove = FALSE;
  if (ctk_tree_selection_get_selected (selection, &model, &iter))
    {
      ctk_tree_model_get (model, &iter, KEYENTRY_COLUMN, &key, -1);
      if (key && key->command != NULL && key->editable)
    can_remove = TRUE;
    }

  ctk_widget_set_sensitive (button, can_remove);
}

static void
cb_app_dialog_response (CtkWidget *dialog, gint response_id, gpointer data)
{
  if (response_id == CTK_RESPONSE_OK)
    {
      GAppInfo *info;
      const gchar *custom_name;

      info = ctk_app_chooser_get_app_info (CTK_APP_CHOOSER (dialog));

      ctk_entry_set_text (CTK_ENTRY (custom_shortcut_command_entry),
                          g_app_info_get_executable (info));
      /* if name isn't set yet, use the associated one */
      custom_name = ctk_entry_get_text (CTK_ENTRY (custom_shortcut_name_entry));
      if (! custom_name || custom_name[0] == '\0')
        ctk_entry_set_text (CTK_ENTRY (custom_shortcut_name_entry),
                            g_app_info_get_display_name (info));

      g_object_unref (info);
    }

  ctk_widget_hide (dialog);
}

static void
setup_dialog (CtkBuilder *builder, GSettings *croma_settings)
{
  CtkCellRenderer *renderer;
  CtkTreeViewColumn *column;
  CtkWidget *widget;
  CtkTreeView *treeview;
  CtkTreeSelection *selection;
  CtkWidget *button;

  treeview = CTK_TREE_VIEW (ctk_builder_get_object (builder,
                                                    "shortcut_treeview"));

  g_signal_connect (treeview, "button_press_event",
            G_CALLBACK (start_editing_cb), builder);
  g_signal_connect (treeview, "row-activated",
            G_CALLBACK (start_editing_kb_cb), NULL);

  renderer = ctk_cell_renderer_text_new ();

  g_signal_connect (renderer, "edited",
                    G_CALLBACK (description_edited_callback),
                    treeview);

  column = ctk_tree_view_column_new_with_attributes (_("Action"),
                             renderer,
                             "text", DESCRIPTION_COLUMN,
                             NULL);
  ctk_tree_view_column_set_cell_data_func (column, renderer, description_set_func, NULL, NULL);
  ctk_tree_view_column_set_resizable (column, FALSE);

  ctk_tree_view_append_column (treeview, column);
  ctk_tree_view_column_set_sort_column_id (column, DESCRIPTION_COLUMN);

  renderer = (CtkCellRenderer *) g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
                           "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
                           NULL);

  g_signal_connect (renderer, "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    treeview);

  g_signal_connect (renderer, "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    treeview);

  column = ctk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, NULL);
  ctk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  ctk_tree_view_column_set_resizable (column, FALSE);

  ctk_tree_view_append_column (treeview, column);
  ctk_tree_view_column_set_sort_column_id (column, KEYENTRY_COLUMN);

  g_signal_connect (croma_settings,
                    "changed::num-workspaces",
                    G_CALLBACK (key_entry_controlling_key_changed),
                    builder);

  /* set up the dialog */
  reload_key_entries (builder);

  widget = _ctk_builder_get_widget (builder, "cafe-keybinding-dialog");
  ctk_window_set_default_size (CTK_WINDOW (widget), 400, 500);
  widget = _ctk_builder_get_widget (builder, "label-suggest");
  ctk_label_set_line_wrap (CTK_LABEL (widget), TRUE);
  ctk_label_set_max_width_chars (CTK_LABEL (widget), 60);

  widget = _ctk_builder_get_widget (builder, "cafe-keybinding-dialog");
  capplet_set_icon (widget, "preferences-desktop-keyboard-shortcuts");
  ctk_widget_show (widget);

  g_signal_connect (widget, "key_press_event", G_CALLBACK (maybe_block_accels), NULL);
  g_signal_connect (widget, "response", G_CALLBACK (cb_dialog_response), builder);

  selection = ctk_tree_view_get_selection (CTK_TREE_VIEW (treeview));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed),
            _ctk_builder_get_widget (builder, "remove-button"));

  /* setup the custom shortcut dialog */
  custom_shortcut_dialog = _ctk_builder_get_widget (builder,
                                                    "custom-shortcut-dialog");
  custom_shortcut_name_entry = _ctk_builder_get_widget (builder,
                                                        "custom-shortcut-name-entry");
  custom_shortcut_command_entry = _ctk_builder_get_widget (builder,
                                                           "custom-shortcut-command-entry");
  ctk_dialog_set_default_response (CTK_DIALOG (custom_shortcut_dialog),
                   CTK_RESPONSE_OK);
  ctk_window_set_transient_for (CTK_WINDOW (custom_shortcut_dialog),
                                CTK_WINDOW (widget));
  button = _ctk_builder_get_widget (builder, "custom-shortcut-command-button");
  widget = _ctk_builder_get_widget (builder, "custom-shortcut-application-dialog");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (ctk_dialog_run), widget);
  g_signal_connect (widget, "response", G_CALLBACK (cb_app_dialog_response), NULL);
  widget = ctk_app_chooser_dialog_get_widget (CTK_APP_CHOOSER_DIALOG (widget));
  ctk_app_chooser_widget_set_show_all (CTK_APP_CHOOSER_WIDGET (widget), TRUE);
}

static void
on_window_manager_change (const char *wm_name, CtkBuilder *builder)
{
  reload_key_entries (builder);
}

int
main (int argc, char *argv[])
{
  CtkBuilder *builder;
  GSettings *croma_settings;

  capplet_init (NULL, &argc, &argv);

  activate_settings_daemon ();

  builder = create_builder ();

  if (!builder) /* Warning was already printed to console */
    exit (EXIT_FAILURE);

  wm_common_register_window_manager_change ((GFunc) on_window_manager_change, builder);

  croma_settings = g_settings_new ("org.cafe.Croma.general");

  setup_dialog (builder, croma_settings);

  ctk_main ();

  g_object_unref (croma_settings);
  g_object_unref (builder);
  return 0;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
