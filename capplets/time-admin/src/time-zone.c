/*
 * Copyright (C) 2022 Pablo Barciela
 * Copyright (C) 2019-2020 MATE Developers
 * Copyright (C) 2018, 2019 zhuyaliang https://github.com/zhuyaliang/
 * Copyright (C) 2010-2018 The GNOME Project
 * Copyright (C) 2010 Intel, Inc
 *
 * Portions from Ubiquity, Copyright (C) 2009 Canonical Ltd.
 * Written by Evan Dandrea <evand@ubuntu.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "time-zone.h"
#include "time-map.h"
#include "time-tool.h"
#include  <math.h>
#define  CAFE_DESKTOP_USE_UNSTABLE_API

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <glib/gi18n.h>

#include <libcafe-desktop/cafe-languages.h>

#define DEFAULT_TZ "Europe/London"
#define BACKFILE   TIMPZONEDIR "backward"

static void LocationChanged(TimezoneMap  *map,
                            TzLocation   *location,
                            TimeAdmin    *ta);

enum {
  CITY_COL_CITY_HUMAN_READABLE,
  CITY_COL_ZONE,
  CITY_NUM_COLS
};

static gchar*
tz_data_file_get (void)
{
    gchar *file;

    file = g_strdup (TZ_DATA_FILE);

    return file;
}

static float
convert_pos (gchar *pos, int digits)
{
    gchar whole[10], *fraction;
    gint i;
    float t1, t2;

    if (!pos || strlen(pos) < 4 || digits > 9)
        return 0.0;

    for (i = 0; i < digits + 1; i++)
        whole[i] = pos[i];

    whole[i] = '\0';
    fraction = pos + digits + 1;

    t1 = g_strtod (whole, NULL);
    t2 = g_strtod (fraction, NULL);

    if (t1 >= 0.0)
        return t1 + t2/pow (10.0, strlen(fraction));
    else
        return t1 - t2/pow (10.0, strlen(fraction));
}

static int compare_country_names (const void *a, const void *b)
{
    const TzLocation *tza = * (TzLocation **) a;
    const TzLocation *tzb = * (TzLocation **) b;

    return strcmp (tza->zone, tzb->zone);
}

static void sort_locations_by_country (GPtrArray *locations)
{
    qsort (locations->pdata, locations->len, sizeof (gpointer),
           compare_country_names);
}
static void load_backward_tz (TzDB *tz_db)
{
    FILE  *fp;
    char buf[128] = { 0 };

    tz_db->backward = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    fp = fopen(BACKFILE,"r");
    if(fp == NULL)
    {
        g_error("%s does not exist\r\n",BACKFILE);

    }
    while(fgets(buf,128,fp))
    {
        g_auto(GStrv) items = NULL;
        guint j;
        char *real, *alias;

        if (g_ascii_strncasecmp (buf, "Link\t", 5) != 0)
            continue;

        items = g_strsplit (buf, "\t", -1);
        real = NULL;
        alias = NULL;
        for (j = 1; items[j] != NULL; j++)
        {
            if (items[j][0] == '\0')
                continue;
            if (real == NULL)
            {
                real = items[j];
                continue;
            }
            alias = items[j];
                break;
        }
        if (real == NULL || alias == NULL)
            g_warning ("Could not parse line: %s", buf);

        /* We don't need more than one name for it */
        if (g_str_equal (real, "Etc/UTC") ||
            g_str_equal (real, "Etc/UCT"))
            real = "Etc/GMT";

        g_hash_table_insert (tz_db->backward, g_strdup (alias), g_strdup (real));

    }
    fclose(fp);
}

TzDB *tz_load_db (void)
{
    g_autofree gchar *tz_data_file = NULL;
    TzDB *tz_db;
    FILE *tzfile;
    char buf[4096];

    tz_data_file = tz_data_file_get ();
    if (!tz_data_file)
    {
        g_warning ("Could not get the TimeZone data file name");
        return NULL;
    }
    tzfile = fopen (tz_data_file, "r");
    if (!tzfile)
    {
        g_warning ("Could not open *%s*\n", tz_data_file);
        return NULL;
    }

    tz_db = g_new0 (TzDB, 1);
    tz_db->locations = g_ptr_array_new ();

    while (fgets (buf, sizeof(buf), tzfile))
    {
        g_auto(GStrv) tmpstrarr = NULL;
        g_autofree gchar *latstr = NULL;
        g_autofree gchar *lngstr = NULL;
        gchar *p;
        TzLocation *loc;

        if (*buf == '#')
            continue;

        g_strchomp(buf);
        tmpstrarr = g_strsplit(buf,"\t", 6);

        latstr = g_strdup (tmpstrarr[1]);
        p = latstr + 1;
        while (*p != '-' && *p != '+')
            p++;
        lngstr = g_strdup (p);
        *p = '\0';

        loc = g_new0 (TzLocation, 1);
        loc->country = g_strdup (tmpstrarr[0]);
        loc->zone = g_strdup (tmpstrarr[2]);
        loc->latitude  = convert_pos (latstr, 2);
        loc->longitude = convert_pos (lngstr, 3);

#ifdef __sun
        if (tmpstrarr[3] && *tmpstrarr[3] == '-' && tmpstrarr[4])
            loc->comment = g_strdup (tmpstrarr[4]);

        if (tmpstrarr[3] && *tmpstrarr[3] != '-' && !islower(loc->zone))
        {
            TzLocation *locgrp;
            locgrp = g_new0 (TzLocation, 1);
            locgrp->country = g_strdup (tmpstrarr[0]);
            locgrp->zone = g_strdup (tmpstrarr[3]);
            locgrp->latitude  = convert_pos (latstr, 2);
            locgrp->longitude = convert_pos (lngstr, 3);
            locgrp->comment = (tmpstrarr[4]) ? g_strdup (tmpstrarr[4]) : NULL;

            g_ptr_array_add (tz_db->locations, (gpointer) locgrp);
        }
#else
        loc->comment = (tmpstrarr[3]) ? g_strdup(tmpstrarr[3]) : NULL;
#endif

        g_ptr_array_add (tz_db->locations, (gpointer) loc);
    }

    fclose (tzfile);

    /* now sort by country */
    sort_locations_by_country (tz_db->locations);

    /* Load up the hashtable of backward links */
    load_backward_tz (tz_db);

    return tz_db;
}

static CtkWidget*
GetTimeZoneMap(TimeAdmin *ta)
{
    CtkWidget *map;
    g_autoptr(CtkEntryCompletion) completion = NULL;

    map = (CtkWidget *) timezone_map_new ();
    g_signal_connect (map,
                     "location-changed",
                      G_CALLBACK (LocationChanged),
                      ta);

    completion = ctk_entry_completion_new ();
    ctk_entry_set_completion (CTK_ENTRY (ta->TimezoneEntry), completion);
    ctk_entry_completion_set_model (completion, CTK_TREE_MODEL (ta->CityListStore));
    ctk_entry_completion_set_text_column (completion, CITY_COL_CITY_HUMAN_READABLE);

    return map;
}
static char *
translated_city_name (TzLocation *loc)
{
    g_autofree gchar *zone_translated = NULL;
    g_auto(GStrv) split_translated = NULL;
    g_autofree gchar *country = NULL;
    gchar *name;
    gint length;

    zone_translated = g_strdup (_(loc->zone));
    g_strdelimit (zone_translated, "_", ' ');
    split_translated = g_regex_split_simple ("[\\x{2044}\\x{2215}\\x{29f8}\\x{ff0f}/]",
                                              zone_translated,
                                              0, 0);

    length = g_strv_length (split_translated);

    country = cafe_get_country_from_code (loc->country, NULL);
    name = g_strdup_printf (C_("timezone loc", "%s, %s"),
                            split_translated[length-1],
                            country);

    return name;
}

static void
update_timezone (TimezoneMap *map)
{
    g_autofree gchar *bubble_text = NULL;
    g_autoptr(GDateTime) current_date = NULL;
    g_autoptr(GTimeZone) timezone = NULL;
    g_autofree gchar *city_country = NULL;
    g_autofree gchar *utc_label = NULL;
    g_autofree gchar *time_label = NULL;
    g_autofree gchar *tz_desc = NULL;
    TzLocation       *current_location;
    GDateTime        *date;

    current_location = timezone_map_get_location (TIMEZONEMAP (map));
    city_country = translated_city_name (current_location);

    timezone = g_time_zone_new (current_location->zone);
    current_date = g_date_time_new_now_local ();
    date = g_date_time_to_timezone (current_date, timezone);

    utc_label = g_date_time_format (date, _("UTC%:::z"));

    tz_desc = g_strdup_printf ( "%s (%s)",
                                g_date_time_get_timezone_abbreviation (date),
                                utc_label);
    time_label = g_date_time_format (date, _("%R"));

    bubble_text = g_strdup_printf ("<b>%s</b>\n"
                                 "<small>%s</small>\n"
                                 "<b>%s</b>",
                                 tz_desc,
                                 city_country,
                                 time_label);
    timezone_map_set_bubble_text (TIMEZONEMAP (map), bubble_text);
    g_date_time_unref(date);
}

static void
LocationChanged(TimezoneMap  *map,
                TzLocation   *location,
                TimeAdmin    *ta)
{
    update_timezone (map);
}

static void
get_initial_timezone (TimeAdmin *ta)
{
    const gchar *timezone;

    timezone = GetTimeZone(ta);

    if (timezone == NULL ||
       !timezone_map_set_timezone (TIMEZONEMAP (ta->map), timezone))
    {
        g_warning ("Timezone '%s' is unhandled,setting %s as default", timezone ? timezone : "(null)", DEFAULT_TZ);
        timezone_map_set_timezone (TIMEZONEMAP (ta->map), DEFAULT_TZ);
    }
    update_timezone (TIMEZONEMAP(ta->map));
}

static void
LoadCities (TzLocation   *loc,
            CtkListStore *CityStore)
{
    g_autofree gchar *human_readable = NULL;

    human_readable = translated_city_name (loc);
    ctk_list_store_insert_with_values (CityStore,
                                       NULL,
                                       0,
                                       CITY_COL_CITY_HUMAN_READABLE,
                                       human_readable,
                                       CITY_COL_ZONE,
                                       loc->zone,
                                       -1);
}

static void
CreateCityList(TimeAdmin *ta)
{
    TzDB *db;
    ta->CityListStore = ctk_list_store_new (CITY_NUM_COLS,G_TYPE_STRING,G_TYPE_STRING);
    db = tz_load_db ();
    g_ptr_array_foreach (db->locations, (GFunc) LoadCities, ta->CityListStore);

    TimeZoneDateBaseFree(db);
}

static CtkWidget*
CreateZoneFrame(TimeAdmin *ta)
{
    CtkWidget *TimeZoneFrame;

    TimeZoneFrame = ctk_frame_new (_("Time Zone"));
    ctk_widget_set_size_request(TimeZoneFrame,300,200);
    ctk_frame_set_shadow_type(CTK_FRAME(TimeZoneFrame),CTK_SHADOW_NONE);

    return TimeZoneFrame;
}

static CtkWidget*
CreateZoneScrolled(TimeAdmin *ta)
{
    CtkWidget *Scrolled;

    Scrolled = ctk_scrolled_window_new (NULL, NULL);

    ctk_scrolled_window_set_policy (CTK_SCROLLED_WINDOW (Scrolled),
                                    CTK_POLICY_AUTOMATIC,
                                    CTK_POLICY_AUTOMATIC);

    ctk_scrolled_window_set_shadow_type (CTK_SCROLLED_WINDOW (Scrolled),
                                         CTK_SHADOW_IN);

    return Scrolled;
}

static void
CreateZoneEntry(TimeAdmin *ta)
{
    CtkWidget *hbox;

    ta->TimezoneEntry = ctk_search_entry_new ();
    hbox = ctk_box_new (CTK_ORIENTATION_HORIZONTAL, 10);
    ctk_widget_set_halign (hbox, CTK_ALIGN_CENTER);
    ctk_box_pack_start (CTK_BOX (hbox), ta->TimezoneEntry, FALSE, FALSE, 0);

    ta->SearchBar = ctk_search_bar_new ();
    ctk_search_bar_connect_entry (CTK_SEARCH_BAR (ta->SearchBar),
                                  CTK_ENTRY (ta->TimezoneEntry));
    ctk_search_bar_set_show_close_button (CTK_SEARCH_BAR (ta->SearchBar),FALSE);
    ctk_container_add (CTK_CONTAINER (ta->SearchBar), hbox);
    ctk_search_bar_set_search_mode(CTK_SEARCH_BAR(ta->SearchBar),TRUE);
}

static gboolean
CityChanged(CtkEntryCompletion *completion,
            CtkTreeModel       *model,
            CtkTreeIter        *iter,
            TimeAdmin          *self)
{
    CtkWidget *entry;
    g_autofree gchar *zone = NULL;

    ctk_tree_model_get (model,
                        iter,
                        CITY_COL_ZONE,
                        &zone,
                        -1);
    timezone_map_set_timezone (TIMEZONEMAP (self->map), zone);

    entry = ctk_entry_completion_get_entry (completion);
    ctk_entry_set_text (CTK_ENTRY (entry), "");

    return TRUE;
}

static void
ChoooseTimezoneDone (CtkWidget *widget,
                     TimeAdmin *ta)
{
    TimezoneMap *map;

    map = TIMEZONEMAP(ta->map);
    SetTimeZone(ta->proxy,map->location->zone);
    ctk_entry_set_text (CTK_ENTRY (ta->TimeZoneEntry), _(map->location->zone));
    ctk_widget_hide_on_delete(CTK_WIDGET(ta->dialog));
}

static void
ChoooseTimezoneClose(CtkWidget  *widget,
                     TimeAdmin  *ta)
{
    ctk_widget_hide_on_delete(CTK_WIDGET(ta->dialog));
}

void SetupTimezoneDialog(TimeAdmin *ta)
{
    CtkWidget *Vbox, *TimeZoneFrame, *Scrolled, *image;

    ta->dialog = ctk_dialog_new_with_buttons (_("Time Zone Selection"),
                                              NULL,
                                              CTK_DIALOG_DESTROY_WITH_PARENT,
                                              NULL,
                                              NULL);
    ctk_window_set_default_size (CTK_WINDOW (ta->dialog), 730, 520);

    ta->TZclose = ctk_button_new_with_mnemonic (_("_Close"));
    image = ctk_image_new_from_icon_name ("window-close", CTK_ICON_SIZE_BUTTON);
    ctk_button_set_image (CTK_BUTTON (ta->TZclose), image);
    ctk_button_set_use_underline (CTK_BUTTON (ta->TZclose), TRUE);
    ctk_style_context_add_class (ctk_widget_get_style_context (ta->TZclose), "text-button");
    ctk_widget_set_can_default (ta->TZclose, TRUE);
    ctk_dialog_add_action_widget (CTK_DIALOG (ta->dialog), ta->TZclose, CTK_RESPONSE_CANCEL);

    ta->TZconfire = ctk_button_new_with_mnemonic (_("Con_firm"));
    image = ctk_image_new_from_icon_name ("emblem-default", CTK_ICON_SIZE_BUTTON);
    ctk_button_set_image (CTK_BUTTON (ta->TZconfire), image);
    ctk_button_set_use_underline (CTK_BUTTON (ta->TZconfire), TRUE);
    ctk_style_context_add_class (ctk_widget_get_style_context (ta->TZconfire), "text-button");
    ctk_widget_set_can_default (ta->TZconfire, TRUE);
    ctk_dialog_add_action_widget (CTK_DIALOG (ta->dialog), ta->TZconfire, CTK_RESPONSE_OK);

    g_signal_connect (ta->TZconfire,
                     "clicked",
                      G_CALLBACK (ChoooseTimezoneDone),
                      ta);

    g_signal_connect (ta->TZclose,
                     "clicked",
                      G_CALLBACK (ChoooseTimezoneClose),
                      ta);

    Vbox = ctk_box_new (CTK_ORIENTATION_VERTICAL, 0);
    ctk_style_context_add_class (ctk_widget_get_style_context (Vbox), "linked");


    TimeZoneFrame = CreateZoneFrame(ta);
    Scrolled = CreateZoneScrolled(ta);
    ctk_container_add (CTK_CONTAINER (TimeZoneFrame), Scrolled);
    CreateCityList(ta);
    CreateZoneEntry(ta);
    ctk_box_pack_start (CTK_BOX (Vbox), ta->SearchBar,FALSE,FALSE, 0);
    ta->map = GetTimeZoneMap(ta);
    ctk_widget_show (ta->map);
    ctk_container_add (CTK_CONTAINER (Scrolled),ta->map);
    ctk_box_pack_start(CTK_BOX(Vbox),TimeZoneFrame,TRUE,TRUE,10);
    get_initial_timezone(ta);

    g_signal_connect(ctk_entry_get_completion (CTK_ENTRY (ta->TimezoneEntry)),
                    "match-selected",
                     G_CALLBACK (CityChanged),
                     ta);

    ctk_box_pack_start (CTK_BOX (ctk_dialog_get_content_area (CTK_DIALOG (ta->dialog))),
                        Vbox,
                        TRUE,
                        TRUE, 8);
}

void tz_info_free (TzInfo *tzinfo)
{
    g_return_if_fail (tzinfo != NULL);

    if (tzinfo->tzname_normal) g_free (tzinfo->tzname_normal);
    if (tzinfo->tzname_daylight) g_free (tzinfo->tzname_daylight);
    g_free (tzinfo);
}

struct {
    const char *orig;
    const char *dest;
} aliases[] = {
    { "Asia/Istanbul",  "Europe/Istanbul" },    /* Istanbul is in both Europe and Asia */
    { "Europe/Nicosia", "Asia/Nicosia" },       /* Ditto */
    { "EET",            "Europe/Istanbul" },    /* Same tz as the 2 above */
    { "HST",            "Pacific/Honolulu" },
    { "WET",            "Europe/Brussels" },    /* Other name for the mainland Europe tz */
    { "CET",            "Europe/Brussels" },    /* ditto */
    { "MET",            "Europe/Brussels" },
    { "Etc/Zulu",       "Etc/GMT" },
    { "Etc/UTC",        "Etc/GMT" },
    { "GMT",            "Etc/GMT" },
    { "Greenwich",      "Etc/GMT" },
    { "Etc/UCT",        "Etc/GMT" },
    { "Etc/GMT0",       "Etc/GMT" },
    { "Etc/GMT+0",      "Etc/GMT" },
    { "Etc/GMT-0",      "Etc/GMT" },
    { "Etc/Universal",  "Etc/GMT" },
    { "PST8PDT",        "America/Los_Angeles" },    /* Other name for the Atlantic tz */
    { "EST",            "America/New_York" },   /* Other name for the Eastern tz */
    { "EST5EDT",        "America/New_York" },   /* ditto */
    { "CST6CDT",        "America/Chicago" },    /* Other name for the Central tz */
    { "MST",            "America/Denver" },     /* Other name for the mountain tz */
    { "MST7MDT",        "America/Denver" },     /* ditto */
};

static gboolean
compare_timezones (const char *a,
                   const char *b)
{
    if (g_str_equal (a, b))
        return TRUE;
    if (strchr (b, '/') == NULL) {
        g_autofree gchar *prefixed = NULL;

        prefixed = g_strdup_printf ("/%s", b);
        if (g_str_has_suffix (a, prefixed))
            return TRUE;
    }

    return FALSE;
}

char *tz_info_get_clean_name (TzDB       *tz_db,
                              const char *tz)
{
    char *ret;
    const char *timezone;
    guint i;
    gboolean replaced;

    /* Remove useless prefixes */
    if (g_str_has_prefix (tz, "right/"))
        tz = tz + strlen ("right/");
    else if (g_str_has_prefix (tz, "posix/"))
        tz = tz + strlen ("posix/");

    /* Here start the crazies */
    replaced = FALSE;

    for (i = 0; i < G_N_ELEMENTS (aliases); i++) {
        if (compare_timezones (tz, aliases[i].orig)) {
            replaced = TRUE;
            timezone = aliases[i].dest;
            break;
        }
    }

    /* Try again! */
    if (!replaced) {
        /* Ignore crazy solar times from the '80s */
        if (g_str_has_prefix (tz, "Asia/Riyadh") ||
            g_str_has_prefix (tz, "Mideast/Riyadh")) {
            timezone = "Asia/Riyadh";
            replaced = TRUE;
        }
    }

    if (!replaced)
        timezone = tz;

    ret = g_hash_table_lookup (tz_db->backward, timezone);
    if (ret == NULL)
        return g_strdup (timezone);
    return g_strdup (ret);
}

TzInfo *tz_info_from_location (TzLocation *loc)
{
    TzInfo *tzinfo;
    time_t curtime;
    struct tm *curzone;
    g_autofree gchar *tz_env_value = NULL;

    g_return_val_if_fail (loc != NULL, NULL);
    g_return_val_if_fail (loc->zone != NULL, NULL);

    tz_env_value = g_strdup (getenv ("TZ"));
    setenv ("TZ", loc->zone, 1);

#if 0
    tzset ();
#endif
    tzinfo = g_new0 (TzInfo, 1);

    curtime = time (NULL);
    curzone = localtime (&curtime);

#ifndef __sun
    tzinfo->tzname_normal = g_strdup (curzone->tm_zone);
    if (curzone->tm_isdst)
        tzinfo->tzname_daylight =
            g_strdup (&curzone->tm_zone[curzone->tm_isdst]);
    else
        tzinfo->tzname_daylight = NULL;

    tzinfo->utc_offset = curzone->tm_gmtoff;
#else
    tzinfo->tzname_normal = NULL;
    tzinfo->tzname_daylight = NULL;
    tzinfo->utc_offset = 0;
#endif

    tzinfo->daylight = curzone->tm_isdst;

    if (tz_env_value)
        setenv ("TZ", tz_env_value, 1);
    else
        unsetenv ("TZ");

    return tzinfo;
}

glong tz_location_get_utc_offset (TzLocation *loc)
{
    TzInfo *tz_info;
    glong offset;

    tz_info = tz_info_from_location (loc);
    offset = tz_info->utc_offset;
    tz_info_free (tz_info);

    return offset;
}

void RunTimeZoneDialog  (CtkButton *button,
                         gpointer   data)
{
    TimeAdmin *ta = (TimeAdmin *)data;

    ctk_widget_show_all(CTK_WIDGET(ta->dialog));
}

static void
tz_location_free (TzLocation *loc, gpointer data)
{
    g_free (loc->country);
    g_free (loc->zone);
    g_free (loc->comment);
    g_free (loc);
}

void TimeZoneDateBaseFree (TzDB *db)
{
    g_ptr_array_foreach (db->locations, (GFunc) tz_location_free, NULL);
    g_ptr_array_free (db->locations, TRUE);
    g_hash_table_destroy (db->backward);
    g_free (db);
}

GPtrArray *tz_get_locations (TzDB *db)
{
    return db->locations;
}
