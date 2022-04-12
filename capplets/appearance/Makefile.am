SUBDIRS = data

BUILT_SOURCES = cafe-appearance-properties-resources.h cafe-appearance-properties-resources.c

# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = appearance

bin_PROGRAMS = cafe-appearance-properties

nodist_cafe_appearance_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_appearance_properties_SOURCES = \
	appearance.h \
	appearance-desktop.c \
	appearance-desktop.h \
	appearance-font.c \
	appearance-font.h \
	appearance-main.c \
	appearance-themes.c \
	appearance-themes.h \
	appearance-style.c \
	appearance-style.h \
	appearance-ui.c \
	appearance-ui.h \
	appearance-support.c \
	appearance-support.h \
	cafe-wp-info.c \
	cafe-wp-info.h \
	cafe-wp-item.c \
	cafe-wp-item.h \
	cafe-wp-xml.c \
	cafe-wp-xml.h \
	theme-installer.c \
	theme-installer.h \
	theme-save.c \
	theme-save.h \
	theme-util.c \
	theme-util.h

AM_CFLAGS = -DCAFE_DESKTOP_USE_UNSTABLE_API

cafe_appearance_properties_LDADD = \
	$(top_builddir)/libwindow-settings/libcafe-window-settings.la \
	$(top_builddir)/capplets/common/libcommon.la \
	$(CAFECC_CAPPLETS_LIBS) \
	$(FONT_CAPPLET_LIBS) \
	$(CROMA_LIBS)
cafe_appearance_properties_LDFLAGS = -export-dynamic

pixmapdir = $(pkgdatadir)/pixmaps
wallpaperdir = $(datadir)/cafe-background-properties
backgrounddir = $(datadir)/backgrounds

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(CROMA_CFLAGS) \
	$(CAFECC_CAPPLETS_CFLAGS) \
	$(FONT_CAPPLET_CFLAGS) \
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DCAFECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	-DWALLPAPER_DATADIR="\"$(wallpaperdir)\"" \
	-DBACKGROUND_DATADIR="\"$(backgrounddir)\""

cafe-appearance-properties-resources.h cafe-appearance-properties-resources.c: org.cafe.mcc.appearance.gresource.xml
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name appearance $<

CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(BUILT_SOURCES)
EXTRA_DIST = org.cafe.mcc.appearance.gresource.xml

-include $(top_srcdir)/git.mk
