#include <config.h>
#include <unistd.h>
#include <string.h>
#include <croma-private/util.h>
#include <croma-private/theme.h>
#include <croma-private/theme-parser.h>
#include <croma-private/preview-widget.h>
#include <signal.h>
#include <errno.h>
#include <math.h>

/* We have to #undef this as croma #defines these. */
#undef _
#undef N_

#include <glib.h>

#include "theme-thumbnail.h"
#include "ctkrc-utils.h"
#include "capplet-util.h"


typedef struct {
	gboolean set;
	gint thumbnail_width;
	gint thumbnail_height;
	GByteArray* data;
	gchar* theme_name;
	ThemeThumbnailFunc func;
	gpointer user_data;
	GDestroyNotify destroy;
	GIOChannel* channel;
	guint watch_id;
} ThemeThumbnailAsyncData;


static ThemeThumbnailAsyncData async_data;

/* Protocol */

/* Our protocol is pretty simple.  The parent process will write several strings
 * (separated by a '\000'). They are the widget theme, the wm theme, the icon
 * theme, etc.  Then, it will wait for the child to write back the data.  The
 * parent expects ICON_SIZE_WIDTH * ICON_SIZE_HEIGHT * 4 bytes of information.
 * After that, the child is ready for the next theme to render.
 */

enum {
	READY_FOR_THEME,
	READING_TYPE,
	READING_CONTROL_THEME_NAME,
	READING_CTK_COLOR_SCHEME,
	READING_WM_THEME_NAME,
	READING_ICON_THEME_NAME,
	READING_APPLICATION_FONT,
	WRITING_PIXBUF_DATA
};

typedef struct {
	gint status;
	GByteArray* type;
	GByteArray* control_theme_name;
	GByteArray* ctk_color_scheme;
	GByteArray* wm_theme_name;
	GByteArray* icon_theme_name;
	GByteArray* application_font;
} ThemeThumbnailData;

typedef struct {
	gchar* thumbnail_type;
	gpointer theme_info;
	ThemeThumbnailFunc func;
	gpointer user_data;
	GDestroyNotify destroy;
} ThemeQueueItem;

static GList* theme_queue = NULL;

static int pipe_to_factory_fd[2];
static int pipe_from_factory_fd[2];

#define THUMBNAIL_TYPE_META     "meta"
#define THUMBNAIL_TYPE_CTK      "ctk"
#define THUMBNAIL_TYPE_CROMA    "croma"
#define THUMBNAIL_TYPE_ICON     "icon"

#define META_THUMBNAIL_SIZE       128
#define CTK_THUMBNAIL_SIZE         96
#define CROMA_THUMBNAIL_WIDTH  120
#define CROMA_THUMBNAIL_HEIGHT  60


static void pixbuf_apply_mask_region(GdkPixbuf* pixbuf, cairo_region_t* region)
{
  gint nchannels, rowstride, w, h;
  guchar *pixels, *p;

  g_return_if_fail (pixbuf);
  g_return_if_fail (region);

  nchannels = cdk_pixbuf_get_n_channels (pixbuf);
  rowstride = cdk_pixbuf_get_rowstride (pixbuf);
  pixels = cdk_pixbuf_get_pixels (pixbuf);


  /* we need an alpha channel ... */
  if (!cdk_pixbuf_get_has_alpha (pixbuf) || nchannels != 4)
    return;

  for (w = 0; w < cdk_pixbuf_get_width (pixbuf); ++w)
    for (h = 0; h < cdk_pixbuf_get_height (pixbuf); ++h)
    {
      if (!cairo_region_contains_point (region, w, h))
      {
        p = pixels + h * rowstride + w * nchannels;
        if (G_BYTE_ORDER == G_BIG_ENDIAN)
          p[0] = 0x0;
        else
          p[3] = 0x0;
      }
    }

}

static GdkPixbuf *
create_folder_icon (char *icon_theme_name)
{
  CtkIconTheme *icon_theme;
  GdkPixbuf *folder_icon = NULL;
  CtkIconInfo *folder_icon_info;
  gchar *example_icon_name;
  const gchar *icon_names[5];
  gint i;

  icon_theme = ctk_icon_theme_new ();
  ctk_icon_theme_set_custom_theme (icon_theme, icon_theme_name);

  i = 0;
  /* Get the Example icon name in the theme if specified */
  example_icon_name = ctk_icon_theme_get_example_icon_name (icon_theme);
  if (example_icon_name != NULL)
    icon_names[i++] = example_icon_name;
  icon_names[i++] = "x-directory-normal";
  icon_names[i++] = "cafe-fs-directory";
  icon_names[i++] = "folder";
  icon_names[i++] = NULL;

  folder_icon_info = ctk_icon_theme_choose_icon (icon_theme, icon_names, 48, CTK_ICON_LOOKUP_FORCE_SIZE);
  if (folder_icon_info != NULL)
  {
    folder_icon = ctk_icon_info_load_icon (folder_icon_info, NULL);
    g_object_unref (folder_icon_info);
  }

  if (folder_icon == NULL)
  {
    folder_icon = ctk_icon_theme_load_icon (icon_theme,
                                            "image-missing",
                                            48, 0, NULL);
  }

  g_object_unref (icon_theme);
  g_free (example_icon_name);

  return folder_icon;
}

static GdkPixbuf *
create_meta_theme_pixbuf (ThemeThumbnailData *theme_thumbnail_data)
{
  CtkWidget *window;
  CtkWidget *preview;
  CtkWidget *vbox;
  CtkWidget *box;
  CtkWidget *image_button;
  CtkWidget *checkbox;
  CtkWidget *radio;

  CtkRequisition requisition;
  CtkAllocation allocation;
  CtkAllocation vbox_allocation;
  MetaFrameFlags flags;
  MetaTheme *theme;
  GdkPixbuf *pixbuf, *icon;
  int icon_width, icon_height;
  cairo_region_t *region;

  g_object_set (ctk_settings_get_default (),
    "ctk-theme-name", (char *) theme_thumbnail_data->control_theme_name->data,
    "ctk-font-name", (char *) theme_thumbnail_data->application_font->data,
    "ctk-icon-theme-name", (char *) theme_thumbnail_data->icon_theme_name->data,
    "ctk-color-scheme", (char *) theme_thumbnail_data->ctk_color_scheme->data,
    NULL);

  theme = meta_theme_load ((char *) theme_thumbnail_data->wm_theme_name->data, NULL);
  if (theme == NULL)
    return NULL;

  /* Represent the icon theme */
  icon = create_folder_icon ((char *) theme_thumbnail_data->icon_theme_name->data);
  icon_width = cdk_pixbuf_get_width (icon);
  icon_height = cdk_pixbuf_get_height (icon);

  /* Create a fake window */
  flags = META_FRAME_ALLOWS_DELETE |
          META_FRAME_ALLOWS_MENU |
          META_FRAME_ALLOWS_MINIMIZE |
          META_FRAME_ALLOWS_MAXIMIZE |
          META_FRAME_ALLOWS_VERTICAL_RESIZE |
          META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
          META_FRAME_HAS_FOCUS |
          META_FRAME_ALLOWS_SHADE |
          META_FRAME_ALLOWS_MOVE;

  window = ctk_offscreen_window_new ();
  preview = meta_preview_new ();
  ctk_container_add (CTK_CONTAINER (window), preview);
  ctk_widget_show_all (window);

  vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 6);
  ctk_container_set_border_width (CTK_CONTAINER (vbox), 6);
  ctk_container_add (CTK_CONTAINER (preview), vbox);

  image_button = ctk_button_new_with_mnemonic (_("_Open"));
  ctk_button_set_image (CTK_BUTTON (image_button), ctk_image_new_from_icon_name ("document-open", CTK_ICON_SIZE_BUTTON));

  ctk_widget_set_halign (image_button, CTK_ALIGN_START);
  ctk_widget_set_valign (image_button, CTK_ALIGN_START);
  ctk_widget_show (image_button);
  ctk_box_pack_start (CTK_BOX (vbox), image_button, FALSE, FALSE, 0);
  box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 0);
  ctk_box_pack_start (CTK_BOX (vbox), box, FALSE, FALSE, 0);
  checkbox = ctk_check_button_new ();
  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (checkbox), TRUE);
  ctk_box_pack_start (CTK_BOX (box), checkbox, FALSE, FALSE, 0);
  radio = ctk_radio_button_new (NULL);
  ctk_box_pack_start (CTK_BOX (box), radio, FALSE, FALSE, 0);

  ctk_widget_show_all (preview);

  meta_preview_set_frame_flags (META_PREVIEW (preview), flags);
  meta_preview_set_theme (META_PREVIEW (preview), theme);
  meta_preview_set_title (META_PREVIEW (preview), "");

  ctk_window_set_default_size (CTK_WINDOW (window), META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE);

  ctk_widget_get_preferred_size (window, &requisition, NULL);
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = META_THUMBNAIL_SIZE;
  allocation.height = META_THUMBNAIL_SIZE;
  ctk_widget_size_allocate (window, &allocation);
  ctk_widget_get_preferred_size (window, &requisition, NULL);

  ctk_widget_queue_draw (window);
  while (ctk_events_pending ())
    ctk_main_iteration ();

  pixbuf = ctk_offscreen_window_get_pixbuf (CTK_OFFSCREEN_WINDOW (window));

  ctk_widget_get_allocation (vbox, &vbox_allocation);

  /* Add the icon theme to the pixbuf */
  cdk_pixbuf_composite (icon, pixbuf,
                        vbox_allocation.x + vbox_allocation.width - icon_width - 5,
                        vbox_allocation.y + vbox_allocation.height - icon_height - 5,
                        icon_width, icon_height,
                        vbox_allocation.x + vbox_allocation.width - icon_width - 5,
                        vbox_allocation.y + vbox_allocation.height - icon_height - 5,
                        1.0, 1.0, GDK_INTERP_BILINEAR, 255);
  region = meta_preview_get_clip_region (META_PREVIEW (preview),
      META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE);
  pixbuf_apply_mask_region (pixbuf, region);
  cairo_region_destroy (region);

  g_object_unref (icon);
  ctk_widget_destroy (window);
  meta_theme_free (theme);

  return pixbuf;
}

static GdkPixbuf *
create_ctk_theme_pixbuf (ThemeThumbnailData *theme_thumbnail_data)
{
  CtkSettings *settings;
  CtkWidget *window, *vbox, *box, *image_button, *checkbox, *radio;
  CtkRequisition requisition;
  CtkAllocation allocation;
  GdkPixbuf *pixbuf, *retval;
  gint width, height;

  settings = ctk_settings_get_default ();
  g_object_set (settings, "ctk-theme-name", (char *) theme_thumbnail_data->control_theme_name->data,
			  "ctk-color-scheme", (char *) theme_thumbnail_data->ctk_color_scheme->data,
 			  NULL);

  window = ctk_offscreen_window_new ();

  vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
  ctk_container_add (CTK_CONTAINER (window), vbox);
  box = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 6);
  ctk_container_set_border_width (CTK_CONTAINER (box), 6);
  ctk_box_pack_start (CTK_BOX (vbox), box, FALSE, FALSE, 0);

  image_button = ctk_button_new_with_mnemonic (_("_Open"));
  ctk_button_set_image (CTK_BUTTON (image_button), ctk_image_new_from_icon_name ("document-open", CTK_ICON_SIZE_BUTTON));

  ctk_box_pack_start (CTK_BOX (box), image_button, FALSE, FALSE, 0);
  checkbox = ctk_check_button_new ();
  ctk_toggle_button_set_active (CTK_TOGGLE_BUTTON (checkbox), TRUE);
  ctk_box_pack_start (CTK_BOX (box), checkbox, FALSE, FALSE, 0);
  radio = ctk_radio_button_new_from_widget (NULL);
  ctk_box_pack_start (CTK_BOX (box), radio, FALSE, FALSE, 0);

  ctk_widget_show_all (window);
  ctk_widget_show_all (vbox);
  ctk_widget_realize (image_button);
  ctk_widget_realize (ctk_bin_get_child (CTK_BIN (image_button)));
  ctk_widget_realize (checkbox);
  ctk_widget_realize (radio);
  ctk_widget_map (image_button);
  ctk_widget_map (ctk_bin_get_child (CTK_BIN (image_button)));
  ctk_widget_map (checkbox);
  ctk_widget_map (radio);

  ctk_widget_get_preferred_size (window, &requisition, NULL);
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = requisition.width;
  allocation.height = requisition.height;
  ctk_widget_size_allocate (window, &allocation);
  ctk_widget_get_preferred_size (window, &requisition, NULL);

  ctk_window_get_size (CTK_WINDOW (window), &width, &height);

  ctk_widget_queue_draw (window);
  while (ctk_events_pending ())
    ctk_main_iteration ();

  pixbuf = ctk_offscreen_window_get_pixbuf (CTK_OFFSCREEN_WINDOW (window));

  retval = cdk_pixbuf_scale_simple (pixbuf,
                                    CTK_THUMBNAIL_SIZE,
                                    (int) CTK_THUMBNAIL_SIZE * (((double) height) / ((double) width)),
                                    GDK_INTERP_BILINEAR);
  g_object_unref (pixbuf);
  ctk_widget_destroy (window);

  return retval;
}

static GdkPixbuf *
create_croma_theme_pixbuf (ThemeThumbnailData *theme_thumbnail_data)
{
  CtkWidget *window, *preview, *dummy;
  MetaFrameFlags flags;
  MetaTheme *theme;
  CtkRequisition requisition;
  CtkAllocation allocation;
  GdkPixbuf *pixbuf, *retval;
  cairo_region_t *region;

  theme = meta_theme_load ((char *) theme_thumbnail_data->wm_theme_name->data, NULL);
  if (theme == NULL)
    return NULL;

  flags = META_FRAME_ALLOWS_DELETE |
          META_FRAME_ALLOWS_MENU |
          META_FRAME_ALLOWS_MINIMIZE |
          META_FRAME_ALLOWS_MAXIMIZE |
          META_FRAME_ALLOWS_VERTICAL_RESIZE |
          META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
          META_FRAME_HAS_FOCUS |
          META_FRAME_ALLOWS_SHADE |
          META_FRAME_ALLOWS_MOVE;

  window = ctk_offscreen_window_new ();
  ctk_window_set_default_size (CTK_WINDOW (window), (int) CROMA_THUMBNAIL_WIDTH * 1.2, (int) CROMA_THUMBNAIL_HEIGHT * 1.2);

  preview = meta_preview_new ();
  meta_preview_set_frame_flags (META_PREVIEW (preview), flags);
  meta_preview_set_theme (META_PREVIEW (preview), theme);
  meta_preview_set_title (META_PREVIEW (preview), "");
  ctk_container_add (CTK_CONTAINER (window), preview);

  dummy = ctk_label_new ("");
  ctk_container_add (CTK_CONTAINER (preview), dummy);

  ctk_widget_show_all (window);

  ctk_widget_get_preferred_size (window, &requisition, NULL);
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = (int) CROMA_THUMBNAIL_WIDTH * 1.2;
  allocation.height = (int) CROMA_THUMBNAIL_HEIGHT * 1.2;
  ctk_widget_size_allocate (window, &allocation);
  ctk_widget_get_preferred_size (window, &requisition, NULL);

  ctk_widget_queue_draw (window);
  while (ctk_events_pending ())
    ctk_main_iteration ();

  pixbuf = ctk_offscreen_window_get_pixbuf (CTK_OFFSCREEN_WINDOW (window));

  region = meta_preview_get_clip_region (META_PREVIEW (preview),
      CROMA_THUMBNAIL_WIDTH * 1.2, CROMA_THUMBNAIL_HEIGHT * 1.2);
  pixbuf_apply_mask_region (pixbuf, region);
  cairo_region_destroy (region);


  retval = cdk_pixbuf_scale_simple (pixbuf,
                                    CROMA_THUMBNAIL_WIDTH,
                                    CROMA_THUMBNAIL_HEIGHT,
                                    GDK_INTERP_BILINEAR);
  g_object_unref (pixbuf);

  ctk_widget_destroy (window);
  meta_theme_free (theme);

  return retval;
}

static GdkPixbuf *
create_icon_theme_pixbuf (ThemeThumbnailData *theme_thumbnail_data)
{
  return create_folder_icon ((char *) theme_thumbnail_data->icon_theme_name->data);
}


static void
handle_bytes (const guint8       *buffer,
              gint                bytes_read,
              ThemeThumbnailData *theme_thumbnail_data)
{
  const guint8 *ptr;
  ptr = buffer;

  while (bytes_read > 0)
  {
    guint8 *nil;

    switch (theme_thumbnail_data->status)
    {
      case READY_FOR_THEME:
        theme_thumbnail_data->status = READING_TYPE;
        /* fall through */
      case READING_TYPE:
        nil = memchr (ptr, '\000', bytes_read);
        if (nil == NULL)
        {
          g_byte_array_append (theme_thumbnail_data->type, ptr, bytes_read);
          bytes_read = 0;
        }
        else
        {
          g_byte_array_append (theme_thumbnail_data->type, ptr, nil - ptr + 1);
          bytes_read -= (nil - ptr + 1);
          ptr = nil + 1;
          theme_thumbnail_data->status = READING_CONTROL_THEME_NAME;
        }
        break;

      case READING_CONTROL_THEME_NAME:
        nil = memchr (ptr, '\000', bytes_read);
        if (nil == NULL)
        {
          g_byte_array_append (theme_thumbnail_data->control_theme_name, ptr, bytes_read);
          bytes_read = 0;
        }
        else
        {
          g_byte_array_append (theme_thumbnail_data->control_theme_name, ptr, nil - ptr + 1);
          bytes_read -= (nil - ptr + 1);
          ptr = nil + 1;
          theme_thumbnail_data->status = READING_CTK_COLOR_SCHEME;
        }
        break;

      case READING_CTK_COLOR_SCHEME:
        nil = memchr (ptr, '\000', bytes_read);
        if (nil == NULL)
        {
          g_byte_array_append (theme_thumbnail_data->ctk_color_scheme, ptr, bytes_read);
          bytes_read = 0;
        }
        else
        {
          g_byte_array_append (theme_thumbnail_data->ctk_color_scheme, ptr, nil - ptr + 1);
          bytes_read -= (nil - ptr + 1);
          ptr = nil + 1;
          theme_thumbnail_data->status = READING_WM_THEME_NAME;
        }
        break;

      case READING_WM_THEME_NAME:
        nil = memchr (ptr, '\000', bytes_read);
        if (nil == NULL)
        {
          g_byte_array_append (theme_thumbnail_data->wm_theme_name, ptr, bytes_read);
          bytes_read = 0;
        }
        else
        {
          g_byte_array_append (theme_thumbnail_data->wm_theme_name, ptr, nil - ptr + 1);
          bytes_read -= (nil - ptr + 1);
          ptr = nil + 1;
          theme_thumbnail_data->status = READING_ICON_THEME_NAME;
        }
        break;

      case READING_ICON_THEME_NAME:
        nil = memchr (ptr, '\000', bytes_read);
        if (nil == NULL)
        {
          g_byte_array_append (theme_thumbnail_data->icon_theme_name, ptr, bytes_read);
          bytes_read = 0;
        }
        else
        {
          g_byte_array_append (theme_thumbnail_data->icon_theme_name, ptr, nil - ptr + 1);
          bytes_read -= (nil - ptr + 1);
          ptr = nil + 1;
          theme_thumbnail_data->status = READING_APPLICATION_FONT;
        }
        break;

      case READING_APPLICATION_FONT:
        nil = memchr (ptr, '\000', bytes_read);
        if (nil == NULL)
        {
          g_byte_array_append (theme_thumbnail_data->application_font, ptr, bytes_read);
          bytes_read = 0;
        }
        else
        {
          g_byte_array_append (theme_thumbnail_data->application_font, ptr, nil - ptr + 1);
          bytes_read -= (nil - ptr + 1);
          ptr = nil + 1;
          theme_thumbnail_data->status = WRITING_PIXBUF_DATA;
        }
        break;

      default:
        g_assert_not_reached ();
    }
  }
}

static gboolean
message_from_capplet (GIOChannel   *source,
                      GIOCondition  condition,
                      gpointer      data)
{
  gchar buffer[1024];
  GIOStatus status;
  gsize bytes_read;
  ThemeThumbnailData *theme_thumbnail_data;

  theme_thumbnail_data = (ThemeThumbnailData *) data;
  status = g_io_channel_read_chars (source,
                                    buffer,
                                    1024,
                                    &bytes_read,
                                    NULL);

  switch (status)
  {
    case G_IO_STATUS_NORMAL:
      handle_bytes ((guint8 *) buffer, bytes_read, theme_thumbnail_data);

      if (theme_thumbnail_data->status == WRITING_PIXBUF_DATA)
      {
        GdkPixbuf *pixbuf = NULL;
        gint i, rowstride;
        guchar *pixels;
        gint width, height;
        const gchar *type = (const gchar *) theme_thumbnail_data->type->data;

        if (!strcmp (type, THUMBNAIL_TYPE_META))
          pixbuf = create_meta_theme_pixbuf (theme_thumbnail_data);
        else if (!strcmp (type, THUMBNAIL_TYPE_CTK))
          pixbuf = create_ctk_theme_pixbuf (theme_thumbnail_data);
        else if (!strcmp (type, THUMBNAIL_TYPE_CROMA))
          pixbuf = create_croma_theme_pixbuf (theme_thumbnail_data);
        else if (!strcmp (type, THUMBNAIL_TYPE_ICON))
          pixbuf = create_icon_theme_pixbuf (theme_thumbnail_data);
        else
          g_assert_not_reached ();

        if (pixbuf == NULL) {
          width = height = rowstride = 0;
          pixels = NULL;
        } else {
          width = cdk_pixbuf_get_width (pixbuf);
          height = cdk_pixbuf_get_height (pixbuf);
          rowstride = cdk_pixbuf_get_rowstride (pixbuf);
          pixels = cdk_pixbuf_get_pixels (pixbuf);
        }

        /* Write the pixbuf's size */

        if (write (pipe_from_factory_fd[1], &width, sizeof (width)) == -1)
          perror ("write error");

        if (write (pipe_from_factory_fd[1], &height, sizeof (height)) == -1)
          perror ("write error");

        for (i = 0; i < height; i++)
        {
          if (write (pipe_from_factory_fd[1], pixels + rowstride * i, width * cdk_pixbuf_get_n_channels (pixbuf)) == -1)
            perror ("write error");
        }

        if (pixbuf)
          g_object_unref (pixbuf);
        g_byte_array_set_size (theme_thumbnail_data->type, 0);
        g_byte_array_set_size (theme_thumbnail_data->control_theme_name, 0);
        g_byte_array_set_size (theme_thumbnail_data->ctk_color_scheme, 0);
        g_byte_array_set_size (theme_thumbnail_data->wm_theme_name, 0);
        g_byte_array_set_size (theme_thumbnail_data->icon_theme_name, 0);
        g_byte_array_set_size (theme_thumbnail_data->application_font, 0);
        theme_thumbnail_data->status = READY_FOR_THEME;
      }
      return TRUE;

    case G_IO_STATUS_AGAIN:
      return TRUE;

    case G_IO_STATUS_EOF:
    case G_IO_STATUS_ERROR:
      _exit (0);

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
generate_next_in_queue (void)
{
  ThemeQueueItem *item;

  if (theme_queue == NULL)
    return;

  item = theme_queue->data;
  theme_queue = g_list_delete_link (theme_queue, g_list_first (theme_queue));

  if (!strcmp (item->thumbnail_type, THUMBNAIL_TYPE_META))
    generate_meta_theme_thumbnail_async ((CafeThemeMetaInfo *) item->theme_info,
                                         item->func,
                                         item->user_data,
                                         item->destroy);
  else if (!strcmp (item->thumbnail_type, THUMBNAIL_TYPE_CTK))
    generate_ctk_theme_thumbnail_async ((CafeThemeInfo *) item->theme_info,
                                        item->func,
                                        item->user_data,
                                        item->destroy);
  else if (!strcmp (item->thumbnail_type, THUMBNAIL_TYPE_CROMA))
    generate_croma_theme_thumbnail_async ((CafeThemeInfo *) item->theme_info,
                                             item->func,
                                             item->user_data,
                                             item->destroy);
  else if (!strcmp (item->thumbnail_type, THUMBNAIL_TYPE_ICON))
    generate_icon_theme_thumbnail_async ((CafeThemeIconInfo *) item->theme_info,
                                         item->func,
                                         item->user_data,
                                         item->destroy);

  g_free (item);
}

static gboolean
message_from_child (GIOChannel   *source,
                    GIOCondition  condition,
                    gpointer      data)
{
  gchar buffer[1024];
  GIOStatus status;
  gsize bytes_read;

  if (async_data.set == FALSE)
    return TRUE;

  if (condition == G_IO_HUP)
    return FALSE;

  status = g_io_channel_read_chars (source,
                                    buffer,
                                    1024,
                                    &bytes_read,
                                    NULL);
  switch (status)
  {
    case G_IO_STATUS_NORMAL:
      g_byte_array_append (async_data.data, (guchar *) buffer, bytes_read);

      if (async_data.thumbnail_width == -1 && async_data.data->len >= 2 * sizeof (gint))
      {
        async_data.thumbnail_width = *((gint *) async_data.data->data);
        async_data.thumbnail_height = *(((gint *) async_data.data->data) + 1);
        g_byte_array_remove_range (async_data.data, 0, 2 * sizeof (gint));
      }

      if (async_data.thumbnail_width >= 0 && async_data.data->len == async_data.thumbnail_width * async_data.thumbnail_height * 4)
      {
        GdkPixbuf *pixbuf = NULL;

        if (async_data.thumbnail_width > 0) {
          gchar *pixels;
          gint i, rowstride;

          pixbuf = cdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, async_data.thumbnail_width, async_data.thumbnail_height);
          pixels = (gchar *) cdk_pixbuf_get_pixels (pixbuf);
          rowstride = cdk_pixbuf_get_rowstride (pixbuf);

          for (i = 0; i < async_data.thumbnail_height; ++i)
            memcpy (pixels + rowstride * i, async_data.data->data + 4 * async_data.thumbnail_width * i, async_data.thumbnail_width * 4);
        }

        /* callback function needs to ref the pixbuf if it wants to keep it */
        (* async_data.func) (pixbuf, async_data.theme_name, async_data.user_data);

        if (async_data.destroy)
          (* async_data.destroy) (async_data.user_data);

        if (pixbuf)
          g_object_unref (pixbuf);

        /* Clean up async_data */
        g_free (async_data.theme_name);
        g_source_remove (async_data.watch_id);
        g_io_channel_unref (async_data.channel);

        /* reset async_data */
        async_data.thumbnail_width = -1;
        async_data.thumbnail_height = -1;
        async_data.theme_name = NULL;
        async_data.channel = NULL;
        async_data.func = NULL;
        async_data.user_data = NULL;
        async_data.destroy = NULL;
        async_data.set = FALSE;
        g_byte_array_set_size (async_data.data, 0);

        generate_next_in_queue ();
      }
      return TRUE;

    case G_IO_STATUS_AGAIN:
      return TRUE;

    case G_IO_STATUS_EOF:
    case G_IO_STATUS_ERROR:
      return FALSE;

    default:
      g_assert_not_reached ();
  }

  return TRUE;
}

static void
send_thumbnail_request (gchar *thumbnail_type,
                        gchar *ctk_theme_name,
                        gchar *ctk_color_scheme,
                        gchar *croma_theme_name,
                        gchar *icon_theme_name,
                        gchar *application_font)
{
  if (write (pipe_to_factory_fd[1], thumbnail_type, strlen (thumbnail_type) + 1) == -1)
    perror ("write error");

  if (ctk_theme_name)
  {
    if (write (pipe_to_factory_fd[1], ctk_theme_name, strlen (ctk_theme_name) + 1) == -1)
      perror ("write error");
  }
  else
  {
    if (write (pipe_to_factory_fd[1], "", 1) == -1)
      perror ("write error");
  }

  if (ctk_color_scheme)
  {
    if (write (pipe_to_factory_fd[1], ctk_color_scheme, strlen (ctk_color_scheme) + 1) == -1)
      perror ("write error");
  }
  else
  {
    if (write (pipe_to_factory_fd[1], "", 1) == -1)
      perror ("write error");
  }

  if (croma_theme_name)
  {
    if (write (pipe_to_factory_fd[1], croma_theme_name, strlen (croma_theme_name) + 1) == -1)
      perror ("write error");
  }
  else
  {
    if (write (pipe_to_factory_fd[1], "", 1) == -1)
      perror ("write error");
  }

  if (icon_theme_name)
  {
    if (write (pipe_to_factory_fd[1], icon_theme_name, strlen (icon_theme_name) + 1) == -1)
      perror ("write error");
  }
  else
  {
    if (write (pipe_to_factory_fd[1], "", 1) == -1)
      perror ("write error");
  }

  if (application_font)
  {
    if (write (pipe_to_factory_fd[1], application_font, strlen (application_font) + 1) == -1)
      perror ("write error");
  }
  else
  {
    if (write (pipe_to_factory_fd[1], "Sans 10", strlen ("Sans 10") + 1) == -1)
      perror ("write error");
  }
}

static GdkPixbuf *
read_pixbuf (void)
{
  gint bytes_read, i, j = 0;
  gint size[2];
  GdkPixbuf *pixbuf;
  gint rowstride;
  guchar *pixels;

  do
  {
    bytes_read = read (pipe_from_factory_fd[0], ((guint8*) size) + j, 2 * sizeof (gint));
    if (bytes_read == 0)
      goto eof;
    j += bytes_read;
  }
  while (j < 2 * sizeof (gint));

  if (size[0] <= 0 || size[1] <= 0)
    return NULL;

  pixbuf = cdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size[0], size[1]);
  rowstride = cdk_pixbuf_get_rowstride (pixbuf);
  pixels = cdk_pixbuf_get_pixels (pixbuf);

  for (i = 0; i < size[1]; i++)
  {
    j = 0;

    do
    {
      bytes_read = read (pipe_from_factory_fd[0], pixels + rowstride * i + j, size[0] * cdk_pixbuf_get_n_channels (pixbuf) - j);

      if (bytes_read > 0)
        j += bytes_read;
      else if (bytes_read == 0)
      {
        g_object_unref (pixbuf);
        goto eof;
      }
    }
    while (j < size[0] * cdk_pixbuf_get_n_channels (pixbuf));
  }

  return pixbuf;

eof:
  g_warning ("Received EOF while reading thumbnail");
  close (pipe_to_factory_fd[1]);
  pipe_to_factory_fd[1] = 0;
  close (pipe_from_factory_fd[0]);
  pipe_from_factory_fd[0] = 0;
  return NULL;
}

static GdkPixbuf *
generate_theme_thumbnail (gchar *thumbnail_type,
                          gchar *ctk_theme_name,
                          gchar *ctk_color_scheme,
                          gchar *croma_theme_name,
                          gchar *icon_theme_name,
                          gchar *application_font)
{
  if (async_data.set || !pipe_to_factory_fd[1] || !pipe_from_factory_fd[0])
    return NULL;

  send_thumbnail_request (thumbnail_type,
                          ctk_theme_name,
                          ctk_color_scheme,
                          croma_theme_name,
                          icon_theme_name,
                          application_font);

  return read_pixbuf ();
}

GdkPixbuf *
generate_meta_theme_thumbnail (CafeThemeMetaInfo *theme_info)
{
  return generate_theme_thumbnail (THUMBNAIL_TYPE_META,
                                   theme_info->ctk_theme_name,
                                   theme_info->ctk_color_scheme,
                                   theme_info->croma_theme_name,
                                   theme_info->icon_theme_name,
                                   theme_info->application_font);
}

GdkPixbuf *
generate_ctk_theme_thumbnail (CafeThemeInfo *theme_info)
{
  gchar *scheme;

  scheme = ctkrc_get_color_scheme_for_theme (theme_info->name);

  return generate_theme_thumbnail (THUMBNAIL_TYPE_CTK,
                                   theme_info->name,
                                   scheme,
                                   NULL,
                                   NULL,
                                   NULL);
  g_free (scheme);
}

GdkPixbuf *
generate_croma_theme_thumbnail (CafeThemeInfo *theme_info)
{
  return generate_theme_thumbnail (THUMBNAIL_TYPE_CROMA,
                                   NULL,
                                   NULL,
                                   theme_info->name,
                                   NULL,
                                   NULL);
}

GdkPixbuf *
generate_icon_theme_thumbnail (CafeThemeIconInfo *theme_info)
{
  return generate_theme_thumbnail (THUMBNAIL_TYPE_ICON,
                                   NULL,
                                   NULL,
                                   NULL,
                                   theme_info->name,
                                   NULL);
}

static void generate_theme_thumbnail_async(gpointer theme_info, gchar* theme_name, gchar* thumbnail_type, gchar* ctk_theme_name, gchar* ctk_color_scheme, gchar* croma_theme_name, gchar* icon_theme_name, gchar* application_font, ThemeThumbnailFunc func, gpointer user_data, GDestroyNotify destroy)
{
	if (async_data.set)
	{
		ThemeQueueItem* item = g_new0 (ThemeQueueItem, 1);

		item->thumbnail_type = thumbnail_type;
		item->theme_info = theme_info;
		item->func = func;
		item->user_data = user_data;
		item->destroy = destroy;

		theme_queue = g_list_append(theme_queue, item);

		return;
	}

	if (!pipe_to_factory_fd[1] || !pipe_from_factory_fd[0])
	{
		(*func)(NULL, theme_name, user_data);

		if (destroy)
		{
			(*destroy)(user_data);
		}

		return;
	}

	if (async_data.channel == NULL)
	{
		async_data.channel = g_io_channel_unix_new(pipe_from_factory_fd[0]);

		g_io_channel_set_flags(async_data.channel, g_io_channel_get_flags (async_data.channel) | G_IO_FLAG_NONBLOCK, NULL);
		g_io_channel_set_encoding(async_data.channel, NULL, NULL);

		async_data.watch_id = g_io_add_watch(async_data.channel, G_IO_IN | G_IO_HUP, message_from_child, NULL);
	}

	async_data.set = TRUE;
	async_data.thumbnail_width = -1;
	async_data.thumbnail_height = -1;
	async_data.theme_name = g_strdup(theme_name);
	async_data.func = func;
	async_data.user_data = user_data;
	async_data.destroy = destroy;

	send_thumbnail_request(thumbnail_type, ctk_theme_name, ctk_color_scheme, croma_theme_name, icon_theme_name, application_font);
}

void
generate_meta_theme_thumbnail_async (CafeThemeMetaInfo *theme_info,
                                     ThemeThumbnailFunc  func,
                                     gpointer            user_data,
                                     GDestroyNotify      destroy)
{
  generate_theme_thumbnail_async (theme_info,
                                         theme_info->name,
                                         THUMBNAIL_TYPE_META,
                                         theme_info->ctk_theme_name,
                                         theme_info->ctk_color_scheme,
                                         theme_info->croma_theme_name,
                                         theme_info->icon_theme_name,
                                         theme_info->application_font,
                                         func, user_data, destroy);
}

void generate_ctk_theme_thumbnail_async (CafeThemeInfo* theme_info, ThemeThumbnailFunc  func, gpointer user_data, GDestroyNotify destroy)
{
	gchar* scheme = ctkrc_get_color_scheme_for_theme(theme_info->name);

	generate_theme_thumbnail_async(theme_info, theme_info->name, THUMBNAIL_TYPE_CTK, theme_info->name, scheme,  NULL, NULL, NULL, func, user_data, destroy);

	g_free(scheme);
}

void
generate_croma_theme_thumbnail_async (CafeThemeInfo *theme_info,
                                         ThemeThumbnailFunc  func,
                                         gpointer            user_data,
                                         GDestroyNotify      destroy)
{
  generate_theme_thumbnail_async (theme_info,
                                         theme_info->name,
                                         THUMBNAIL_TYPE_CROMA,
                                         NULL,
                                         NULL,
                                         theme_info->name,
                                         NULL,
                                         NULL,
                                         func, user_data, destroy);
}

void
generate_icon_theme_thumbnail_async (CafeThemeIconInfo *theme_info,
                                     ThemeThumbnailFunc  func,
                                     gpointer            user_data,
                                     GDestroyNotify      destroy)
{
  generate_theme_thumbnail_async (theme_info,
                                         theme_info->name,
                                         THUMBNAIL_TYPE_ICON,
                                         NULL,
                                         NULL,
                                         NULL,
                                         theme_info->name,
                                         NULL,
                                         func, user_data, destroy);
}

void
theme_thumbnail_factory_init (int argc, char *argv[])
{
  gint child_pid;

  if (pipe (pipe_to_factory_fd) == -1)
    perror ("pipe error");

  if (pipe (pipe_from_factory_fd) == -1)
    perror ("pipe error");

  child_pid = fork ();
  if (child_pid == 0)
  {
    ThemeThumbnailData data;
    GIOChannel *channel;

    /* Child */
    ctk_init (&argc, &argv);

    close (pipe_to_factory_fd[1]);
    pipe_to_factory_fd[1] = 0;
    close (pipe_from_factory_fd[0]);
    pipe_from_factory_fd[0] = 0;

    data.status = READY_FOR_THEME;
    data.type = g_byte_array_new ();
    data.control_theme_name = g_byte_array_new ();
    data.ctk_color_scheme = g_byte_array_new ();
    data.wm_theme_name = g_byte_array_new ();
    data.icon_theme_name = g_byte_array_new ();
    data.application_font = g_byte_array_new ();

    channel = g_io_channel_unix_new (pipe_to_factory_fd[0]);
    g_io_channel_set_flags (channel, g_io_channel_get_flags (channel) |
          G_IO_FLAG_NONBLOCK, NULL);
    g_io_channel_set_encoding (channel, NULL, NULL);
    g_io_add_watch (channel, G_IO_IN | G_IO_HUP, message_from_capplet, &data);
    g_io_channel_unref (channel);

    ctk_main ();
    _exit (0);
  }

  g_assert (child_pid > 0);

  /* Parent */
  close (pipe_to_factory_fd[0]);
  close (pipe_from_factory_fd[1]);

  async_data.set = FALSE;
  async_data.theme_name = NULL;
  async_data.data = g_byte_array_new ();
}
