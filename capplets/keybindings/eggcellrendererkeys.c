#include <config.h>
#include <libintl.h>
#include <ctk/ctk.h>
#include <cdk/cdkx.h>
#include <cdk/cdkkeysyms.h>
#include "eggcellrendererkeys.h"
#include "eggaccelerators.h"

#ifndef EGG_COMPILATION
	#ifndef _
		#define _(x) dgettext (GETTEXT_PACKAGE, x)
		#define N_(x) x
	#endif
#else
	#define _(x) x
	#define N_(x) x
#endif

#define EGG_CELL_RENDERER_TEXT_PATH "egg-cell-renderer-text"

#define TOOLTIP_TEXT _("New shortcut...")

static void             egg_cell_renderer_keys_finalize      (GObject             *object);
static void             egg_cell_renderer_keys_init          (EggCellRendererKeys *cell_keys);
static void             egg_cell_renderer_keys_class_init    (EggCellRendererKeysClass *cell_keys_class);
static CtkCellEditable *egg_cell_renderer_keys_start_editing (CtkCellRenderer          *cell,
							      CdkEvent                 *event,
							      CtkWidget                *widget,
							      const gchar              *path,
							      const CdkRectangle       *background_area,
							      const CdkRectangle       *cell_area,
							      CtkCellRendererState      flags);


static void egg_cell_renderer_keys_get_property (GObject         *object,
						 guint            param_id,
						 GValue          *value,
						 GParamSpec      *pspec);
static void egg_cell_renderer_keys_set_property (GObject         *object,
						 guint            param_id,
						 const GValue    *value,
						 GParamSpec      *pspec);
static void egg_cell_renderer_keys_get_size	(CtkCellRenderer    *cell,
						 CtkWidget          *widget,
						 const CdkRectangle *cell_area,
						 gint               *x_offset,
						 gint               *y_offset,
						 gint               *width,
						 gint               *height);


enum {
  PROP_0,

  PROP_ACCEL_KEY,
  PROP_ACCEL_MASK,
  PROP_KEYCODE,
  PROP_ACCEL_MODE
};

static CtkCellRendererTextClass* parent_class = NULL;

GType egg_cell_renderer_keys_get_type(void)
{
	static GType cell_keys_type = 0;

	if (!cell_keys_type)
	{
		static const GTypeInfo cell_keys_info = {
			sizeof (EggCellRendererKeysClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc)egg_cell_renderer_keys_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (EggCellRendererKeys),
			0,              /* n_preallocs */
			(GInstanceInitFunc) egg_cell_renderer_keys_init
		};

	  cell_keys_type = g_type_register_static (CTK_TYPE_CELL_RENDERER_TEXT, "EggCellRendererKeys", &cell_keys_info, 0);
	}

	return cell_keys_type;
}

static void egg_cell_renderer_keys_init(EggCellRendererKeys* cell_keys)
{
	cell_keys->accel_mode = EGG_CELL_RENDERER_KEYS_MODE_CTK;
}

/* FIXME setup stuff to generate this */
/* VOID:STRING,UINT,FLAGS,UINT */
static void marshal_VOID__STRING_UINT_FLAGS_UINT(GClosure* closure, GValue* return_value, guint n_param_values, const GValue* param_values, gpointer invocation_hint, gpointer marshal_data)
{
	/* typedef inside a function? wow */
	typedef void (*GMarshalFunc_VOID__STRING_UINT_FLAGS_UINT) (
		gpointer data1,
		const char* arg_1,
		guint arg_2,
		int arg_3,
		guint arg_4,
		gpointer data2);

	register GMarshalFunc_VOID__STRING_UINT_FLAGS_UINT callback;
	register GCClosure* cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail (n_param_values == 5);

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	callback = (GMarshalFunc_VOID__STRING_UINT_FLAGS_UINT) (marshal_data ? marshal_data : cc->callback);

	callback(data1,
		g_value_get_string(param_values + 1),
		g_value_get_uint(param_values + 2),
		g_value_get_flags(param_values + 3),
		g_value_get_uint(param_values + 4),
		data2);
}

static void
egg_cell_renderer_keys_class_init (EggCellRendererKeysClass *cell_keys_class)
{
  GObjectClass *object_class;
  CtkCellRendererClass *cell_renderer_class;

  object_class = G_OBJECT_CLASS (cell_keys_class);
  cell_renderer_class = CTK_CELL_RENDERER_CLASS (cell_keys_class);
  parent_class = g_type_class_peek_parent (object_class);

  CTK_CELL_RENDERER_CLASS (cell_keys_class)->start_editing = egg_cell_renderer_keys_start_editing;

  object_class->set_property = egg_cell_renderer_keys_set_property;
  object_class->get_property = egg_cell_renderer_keys_get_property;
  cell_renderer_class->get_size = egg_cell_renderer_keys_get_size;

  object_class->finalize = egg_cell_renderer_keys_finalize;

  /* FIXME if this gets moved to a real library, rename the properties
   * to match whatever the CTK convention is
   */

  g_object_class_install_property (object_class,
                                   PROP_ACCEL_KEY,
                                   g_param_spec_uint ("accel_key",
                                                     _("Accelerator key"),
                                                     _("Accelerator key"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MASK,
                                   g_param_spec_flags ("accel_mask",
                                                       _("Accelerator modifiers"),
                                                       _("Accelerator modifiers"),
                                                       CDK_TYPE_MODIFIER_TYPE,
                                                       0,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
		  		   PROP_KEYCODE,
				   g_param_spec_uint ("keycode",
					   	      _("Accelerator keycode"),
						      _("Accelerator keycode"),
						      0,
						      G_MAXINT,
						      0,
						      G_PARAM_READABLE | G_PARAM_WRITABLE));

  /* FIXME: Register the enum when moving to CTK+ */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODE,
                                   g_param_spec_int ("accel_mode",
						     _("Accel Mode"),
						     _("The type of accelerator."),
						     0,
						     2,
						     0,
						     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_signal_new ("accel_edited",
                EGG_TYPE_CELL_RENDERER_KEYS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EggCellRendererKeysClass, accel_edited),
                NULL, NULL,
                marshal_VOID__STRING_UINT_FLAGS_UINT,
                G_TYPE_NONE, 4,
                G_TYPE_STRING,
                G_TYPE_UINT,
                CDK_TYPE_MODIFIER_TYPE,
		G_TYPE_UINT);

  g_signal_new ("accel_cleared",
                EGG_TYPE_CELL_RENDERER_KEYS,
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (EggCellRendererKeysClass, accel_cleared),
                NULL, NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE, 1,
		G_TYPE_STRING);
}


CtkCellRenderer* egg_cell_renderer_keys_new(void)
{
	return CTK_CELL_RENDERER(g_object_new(EGG_TYPE_CELL_RENDERER_KEYS, NULL));
}

static void egg_cell_renderer_keys_finalize(GObject* object)
{
	(*G_OBJECT_CLASS(parent_class)->finalize)(object);
}

static gchar* convert_keysym_state_to_string(guint keysym, guint keycode, EggVirtualModifierType mask)
{
	if (keysym == 0 && keycode == 0)
	{
		return g_strdup (_("Disabled"));
	}
	else
	{
		return egg_virtual_accelerator_label(keysym, keycode, mask);
	}
}

static void
egg_cell_renderer_keys_get_property  (GObject                  *object,
                                      guint                     param_id,
                                      GValue                   *value,
                                      GParamSpec               *pspec)
{
  EggCellRendererKeys *keys;

  g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

  keys = EGG_CELL_RENDERER_KEYS (object);

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      g_value_set_uint (value, keys->accel_key);
      break;

    case PROP_ACCEL_MASK:
      g_value_set_flags (value, keys->accel_mask);
      break;

    case PROP_ACCEL_MODE:
      g_value_set_int (value, keys->accel_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
egg_cell_renderer_keys_set_property  (GObject                  *object,
                                      guint                     param_id,
                                      const GValue             *value,
                                      GParamSpec               *pspec)
{
  EggCellRendererKeys *keys;

  g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (object));

  keys = EGG_CELL_RENDERER_KEYS (object);

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      egg_cell_renderer_keys_set_accelerator (keys,
                                              g_value_get_uint (value),
					      keys->keycode,
                                              keys->accel_mask);
      break;

    case PROP_ACCEL_MASK:
      egg_cell_renderer_keys_set_accelerator (keys,
                                              keys->accel_key,
					      keys->keycode,
                                              g_value_get_flags (value));
      break;
    case PROP_KEYCODE:
      egg_cell_renderer_keys_set_accelerator (keys,
		      			      keys->accel_key,
					      g_value_get_uint (value),
					      keys->accel_mask);
      break;

    case PROP_ACCEL_MODE:
      egg_cell_renderer_keys_set_accel_mode (keys, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static gboolean is_modifier(guint keycode)
{
	gint i;
	gint map_size;
	XModifierKeymap* mod_keymap;
	gboolean retval = FALSE;

	mod_keymap = XGetModifierMapping(cdk_x11_display_get_xdisplay(cdk_display_get_default()));

	map_size = 8 * mod_keymap->max_keypermod;
	i = 0;

	while (i < map_size)
	{
		if (keycode == mod_keymap->modifiermap[i])
		{
			retval = TRUE;
			break;
		}

		++i;
	}

	XFreeModifiermap(mod_keymap);

	return retval;
}

static void
egg_cell_renderer_keys_get_size(CtkCellRenderer    *cell,
				 CtkWidget          *widget,
				 const CdkRectangle *cell_area,
				 gint               *x_offset,
				 gint               *y_offset,
				 gint               *width,
				 gint               *height)
{
  EggCellRendererKeys *keys = (EggCellRendererKeys *) cell;
  CtkRequisition requisition;

  if (keys->sizing_label == NULL)
    keys->sizing_label = ctk_label_new (TOOLTIP_TEXT);

  ctk_widget_get_preferred_size (keys->sizing_label, &requisition, NULL);
  (* CTK_CELL_RENDERER_CLASS (parent_class)->get_size) (cell, widget, cell_area, x_offset, y_offset, width, height);
  /* FIXME: need to take the cell_area et al. into account */
  if (width)
    *width = MAX (*width, requisition.width);
  if (height)
    *height = MAX (*height, requisition.height);
}

/* FIXME: Currently we don't differentiate between a 'bogus' key (like tab in
 * CTK mode) and a removed key.
 */

static gboolean grab_key_callback(CtkWidget* widget, CdkEventKey* event, void* data)
{
	CdkModifierType accel_mods = 0;
	guint accel_keyval;
	EggCellRendererKeys *keys;
	char *path;
	gboolean edited;
	gboolean cleared;
	CdkModifierType consumed_modifiers;
	guint upper;
	CdkModifierType ignored_modifiers;
	CdkDisplay *display;
	CdkSeat *seat;

	keys = EGG_CELL_RENDERER_KEYS(data);

	if (is_modifier(event->hardware_keycode))
	{
		return TRUE;
	}

	edited = FALSE;
	cleared = FALSE;

	consumed_modifiers = 0;
	cdk_keymap_translate_keyboard_state (cdk_keymap_get_for_display (cdk_display_get_default ()),
		event->hardware_keycode,
		event->state,
		event->group,
		NULL, NULL, NULL, &consumed_modifiers);

	upper = event->keyval;
	accel_keyval = cdk_keyval_to_lower(upper);

	if (accel_keyval == CDK_KEY_ISO_Left_Tab)
	{
		accel_keyval = CDK_KEY_Tab;
	}

	/* Put shift back if it changed the case of the key, not otherwise. */
	if (upper != accel_keyval && (consumed_modifiers & CDK_SHIFT_MASK))
	{
		consumed_modifiers &= ~(CDK_SHIFT_MASK);
	}

	egg_keymap_resolve_virtual_modifiers (cdk_keymap_get_for_display (cdk_display_get_default ()),
		EGG_VIRTUAL_NUM_LOCK_MASK |
		EGG_VIRTUAL_SCROLL_LOCK_MASK |
		EGG_VIRTUAL_LOCK_MASK,
		&ignored_modifiers);

	/* http://bugzilla.gnome.org/show_bug.cgi?id=139605
	 * mouse keys should effect keybindings */
	ignored_modifiers |= CDK_BUTTON1_MASK |
		CDK_BUTTON2_MASK |
		CDK_BUTTON3_MASK |
		CDK_BUTTON4_MASK |
		CDK_BUTTON5_MASK;

	/* filter consumed/ignored modifiers */
	if (keys->accel_mode == EGG_CELL_RENDERER_KEYS_MODE_CTK)
	{
		accel_mods = event->state & CDK_MODIFIER_MASK & ~(consumed_modifiers | ignored_modifiers);
	}
	else if (keys->accel_mode == EGG_CELL_RENDERER_KEYS_MODE_X)
	{
		accel_mods = event->state & CDK_MODIFIER_MASK & ~(ignored_modifiers);
	}
	else
	{
		g_assert_not_reached();
	}

	if (accel_mods == 0 && accel_keyval == CDK_KEY_Escape)
	{
		goto out; /* cancel */
	}

	/* clear the accelerator on Backspace */
	if (accel_mods == 0 && accel_keyval == CDK_KEY_BackSpace)
	{
		cleared = TRUE;
		goto out;
	}

	if (keys->accel_mode == EGG_CELL_RENDERER_KEYS_MODE_CTK)
	{
		if (!ctk_accelerator_valid (accel_keyval, accel_mods))
		{
			accel_keyval = 0;
			accel_mods = 0;
		}
	}

	edited = TRUE;

	out:

	display = ctk_widget_get_display (widget);
	seat = cdk_display_get_default_seat (display);

	cdk_seat_ungrab (seat);

	path = g_strdup(g_object_get_data(G_OBJECT(keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH));

	ctk_cell_editable_editing_done(CTK_CELL_EDITABLE(keys->edit_widget));
	ctk_cell_editable_remove_widget(CTK_CELL_EDITABLE(keys->edit_widget));
	keys->edit_widget = NULL;
	keys->grab_widget = NULL;

	if (edited)
	{
		g_signal_emit_by_name(G_OBJECT(keys), "accel_edited", path, accel_keyval, accel_mods, event->hardware_keycode);
	}
	else if (cleared)
	{
		g_signal_emit_by_name(G_OBJECT(keys), "accel_cleared", path);
	}

	g_free (path);
	return TRUE;
}

static void ungrab_stuff(CtkWidget* widget, gpointer data)
{
	EggCellRendererKeys* keys = EGG_CELL_RENDERER_KEYS(data);
	CdkDisplay *display;
	CdkSeat *seat;

	display = ctk_widget_get_display (widget);
	seat = cdk_display_get_default_seat (display);

	cdk_seat_ungrab (seat);

	g_signal_handlers_disconnect_by_func(G_OBJECT(keys->grab_widget), G_CALLBACK(grab_key_callback), data);
}

static void pointless_eventbox_start_editing(CtkCellEditable* cell_editable, CdkEvent* event)
{
	/* do nothing, because we are pointless */
}

static void pointless_eventbox_cell_editable_init(CtkCellEditableIface* iface)
{
	iface->start_editing = pointless_eventbox_start_editing;
}

static GType
pointless_eventbox_subclass_get_type (void)
{
  static GType eventbox_type = 0;

  if (!eventbox_type)
    {
      static const GTypeInfo eventbox_info =
      {
        sizeof (CtkEventBoxClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (CtkEventBox),
	0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };

      static const GInterfaceInfo cell_editable_info = {
        (GInterfaceInitFunc) pointless_eventbox_cell_editable_init,
        NULL, NULL };

      eventbox_type = g_type_register_static (CTK_TYPE_EVENT_BOX, "EggCellEditableEventBox", &eventbox_info, 0);

      g_type_add_interface_static (eventbox_type,
				   CTK_TYPE_CELL_EDITABLE,
				   &cell_editable_info);
    }

  return eventbox_type;
}

static void
override_background_color (CtkWidget *widget,
                           CdkRGBA   *rgba)
{
  gchar          *css;
  CtkCssProvider *provider;

  provider = ctk_css_provider_new ();

  css = g_strdup_printf ("* { background-color: %s;}",
                         cdk_rgba_to_string (rgba));
  ctk_css_provider_load_from_data (provider, css, -1, NULL);
  g_free (css);

  ctk_style_context_add_provider (ctk_widget_get_style_context (widget),
                                  CTK_STYLE_PROVIDER (provider),
                                  CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

static void
override_color (CtkWidget *widget,
                CdkRGBA   *rgba)
{
  gchar          *css;
  CtkCssProvider *provider;

  provider = ctk_css_provider_new ();

  css = g_strdup_printf ("* { color: %s;}",
                         cdk_rgba_to_string (rgba));
  ctk_css_provider_load_from_data (provider, css, -1, NULL);
  g_free (css);

  ctk_style_context_add_provider (ctk_widget_get_style_context (widget),
                                  CTK_STYLE_PROVIDER (provider),
                                  CTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

static CtkCellEditable *
egg_cell_renderer_keys_start_editing (CtkCellRenderer      *cell,
				      CdkEvent             *event,
				      CtkWidget            *widget,
				      const gchar          *path,
				      const CdkRectangle   *background_area,
				      const CdkRectangle   *cell_area,
				      CtkCellRendererState  flags)
{
  CtkCellRendererText *celltext;
  EggCellRendererKeys *keys;
  CdkDisplay *display;
  CdkSeat *seat;
  CtkWidget *label;
  CtkWidget *eventbox;
  GValue celltext_editable = {0};
  CdkRGBA box_color;
  CdkRGBA label_color;
  CdkRGBA *c;

  celltext = CTK_CELL_RENDERER_TEXT (cell);
  keys = EGG_CELL_RENDERER_KEYS (cell);

  /* If the cell isn't editable we return NULL. */
  g_value_init (&celltext_editable, G_TYPE_BOOLEAN);
  g_object_get_property (G_OBJECT (celltext), "editable", &celltext_editable);
  if (g_value_get_boolean (&celltext_editable) == FALSE)
    return NULL;
  g_return_val_if_fail (ctk_widget_get_window (widget) != NULL, NULL);

  display = ctk_widget_get_display (widget);
  seat = cdk_display_get_default_seat (display);

  if (cdk_seat_grab (seat,
                     ctk_widget_get_window (widget),
                     CDK_SEAT_CAPABILITY_ALL,
                     FALSE,
                     NULL,
                     event,
                     NULL,
                     NULL) != CDK_GRAB_SUCCESS)
    return NULL;

  keys->grab_widget = widget;

  g_signal_connect (G_OBJECT (widget), "key_press_event",
                    G_CALLBACK (grab_key_callback),
                    keys);

  eventbox = g_object_new (pointless_eventbox_subclass_get_type (),
                           NULL);
  keys->edit_widget = eventbox;
  g_object_add_weak_pointer (G_OBJECT (keys->edit_widget),
                             (void**) &keys->edit_widget);

  label = ctk_label_new (NULL);
  ctk_label_set_xalign (CTK_LABEL (label), 0.0);

  ctk_style_context_get (ctk_widget_get_style_context (widget), CTK_STATE_INSENSITIVE,
                         CTK_STYLE_PROPERTY_BACKGROUND_COLOR,
                         &c, NULL);
  box_color = *c;
  cdk_rgba_free (c);

  override_background_color (eventbox, &box_color);

  ctk_style_context_get_color (ctk_widget_get_style_context (widget),
                               CTK_STATE_INSENSITIVE,
                               &label_color);

  override_color (label, &label_color);

  ctk_label_set_text (CTK_LABEL (label),
		  TOOLTIP_TEXT);

  ctk_container_add (CTK_CONTAINER (eventbox), label);

  g_object_set_data_full (G_OBJECT (keys->edit_widget), EGG_CELL_RENDERER_TEXT_PATH,
                          g_strdup (path), g_free);

  ctk_widget_show_all (keys->edit_widget);

  g_signal_connect (G_OBJECT (keys->edit_widget), "unrealize",
                    G_CALLBACK (ungrab_stuff), keys);

  keys->edit_key = keys->accel_key;

  return CTK_CELL_EDITABLE (keys->edit_widget);
}

void egg_cell_renderer_keys_set_accelerator(EggCellRendererKeys* keys, guint keyval, guint keycode, EggVirtualModifierType mask)
{
	char *text;
	gboolean changed;

	g_return_if_fail (EGG_IS_CELL_RENDERER_KEYS (keys));

	g_object_freeze_notify (G_OBJECT (keys));

	changed = FALSE;

	if (keyval != keys->accel_key)
	{
		keys->accel_key = keyval;
		g_object_notify (G_OBJECT (keys), "accel_key");
		changed = TRUE;
	}

	if (mask != keys->accel_mask)
	{
		keys->accel_mask = mask;

		g_object_notify (G_OBJECT (keys), "accel_mask");
		changed = TRUE;
	}

	if (keycode != keys->keycode)
	{
		keys->keycode = keycode;

		g_object_notify (G_OBJECT (keys), "keycode");
		changed = TRUE;
	}

	g_object_thaw_notify (G_OBJECT (keys));

	if (changed)
	{
		/* sync string to the key values */
		text = convert_keysym_state_to_string (keys->accel_key, keys->keycode, keys->accel_mask);
		g_object_set (keys, "text", text, NULL);
		g_free (text);
	}
}

void egg_cell_renderer_keys_get_accelerator(EggCellRendererKeys* keys, guint* keyval, EggVirtualModifierType* mask)
{
	g_return_if_fail(EGG_IS_CELL_RENDERER_KEYS(keys));

	if (keyval)
	{
		*keyval = keys->accel_key;
	}

	if (mask)
	{
		*mask = keys->accel_mask;
	}
}

void egg_cell_renderer_keys_set_accel_mode (EggCellRendererKeys* keys, EggCellRendererKeysMode accel_mode)
{
	g_return_if_fail(EGG_IS_CELL_RENDERER_KEYS(keys));
	keys->accel_mode = accel_mode;
	g_object_notify(G_OBJECT(keys), "accel_mode");
}
