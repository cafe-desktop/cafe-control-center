#include <config.h>
#include <ctk/ctk.h>
#include <string.h>
#include "cafe-theme-info.h"

int
main (int argc, char *argv[])
{
  GList *themes, *list;

  ctk_init (&argc, &argv);
  cafe_theme_init ();

  themes = cafe_theme_meta_info_find_all ();
  if (themes == NULL)
    {
      g_print ("No meta themes were found.\n");
    }
  else
    {
      g_print ("%d meta themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  CafeThemeMetaInfo *meta_theme_info;

	  meta_theme_info = list->data;
	  g_print ("\t%s\n", meta_theme_info->readable_name);
	}
    }
  g_list_free (themes);

  themes = cafe_theme_icon_info_find_all ();
  if (themes == NULL)
    {
      g_print ("No icon themes were found.\n");
    }
  else
    {
      g_print ("%d icon themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  CafeThemeIconInfo *icon_theme_info;

	  icon_theme_info = list->data;
	  g_print ("\t%s\n", icon_theme_info->name);
	}
    }
  g_list_free (themes);

  themes = cafe_theme_info_find_by_type (CAFE_THEME_CROMA);
  if (themes == NULL)
    {
      g_print ("No croma themes were found.\n");
    }
  else
    {
      g_print ("%d croma themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  CafeThemeInfo *theme_info;

	  theme_info = list->data;
	  g_print ("\t%s\n", theme_info->name);
	}
    }
  g_list_free (themes);

  themes = cafe_theme_info_find_by_type (CAFE_THEME_CTK_2);
  if (themes == NULL)
    {
      gchar *str;

      g_print ("No ctk-2 themes were found.  The following directories were tested:\n");
      str = ctk_rc_get_theme_dir ();
      g_print ("\t%s\n", str);
      g_free (str);
      str = g_build_filename (g_get_home_dir (), ".themes", NULL);
      g_print ("\t%s\n", str);
      g_free (str);
    }
  else
    {
      g_print ("%d ctk-2 themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  CafeThemeInfo *theme_info;

	  theme_info = list->data;
	  g_print ("\t%s\n", theme_info->name);
	}
    }
  g_list_free (themes);

  themes = cafe_theme_info_find_by_type (CAFE_THEME_CTK_2_KEYBINDING);
  if (themes == NULL)
    {
      g_print ("No keybinding themes were found.\n");
    }
  else
    {
      g_print ("%d keybinding themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  CafeThemeInfo *theme_info;

	  theme_info = list->data;
	  g_print ("\t%s\n", theme_info->name);
	}
    }
  g_list_free (themes);

  themes = cafe_theme_cursor_info_find_all ();
  if (themes == NULL)
    {
      g_print ("No cursor themes were found.\n");
    }
  else
    {
      g_print ("%d cursor themes were found:\n", g_list_length (themes));
      for (list = themes; list; list = list->next)
	{
	  CafeThemeCursorInfo *cursor_theme_info;

	  cursor_theme_info = list->data;
	  g_print ("\t%s\n", cursor_theme_info->name);
	}
    }
  g_list_free (themes);

  return 0;
}

