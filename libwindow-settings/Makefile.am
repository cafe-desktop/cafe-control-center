WM_MODULE_DIR=$(libdir)/window-manager-settings

AM_CPPFLAGS = 								\
	-DCAFELOCALEDIR="\"$(datadir)/locale\""				\
	-DCAFE_ICONDIR=\""$(datadir)/pixmaps"\"				\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DCAFE_WM_PROPERTY_PATH=\"$(datadir)/cafe/wm-properties\"	\
	-DCAFE_WINDOW_MANAGER_MODULE_PATH=\""$(WM_MODULE_DIR)"\"	\
	-I$(top_srcdir)/						\
	$(WARN_CFLAGS)							\
	@CAPPLET_CFLAGS@						\
	-DMARCO_THEME_DIR=\""$(datadir)/themes"\"

lib_LTLIBRARIES = libcafe-window-settings.la

libcafe_window_settings_la_LDFLAGS = 	\
	-export_dynamic			\
	-no-undefined			\
        -version-info 1:0:0

libcafe_window_settings_la_LIBADD = $(CAFECC_LIBS)

libcafe_window_settings_la_SOURCES = \
	cafe-window-manager.c	\
	cafe-window-manager.h	\
	cafe-wm-manager.c	\
	cafe-wm-manager.h

libcafe_window_settingsincludedir = $(includedir)/cafe-window-settings-2.0

libcafe_window_settingsinclude_HEADERS =	\
	cafe-window-manager.h			\
	cafe-wm-manager.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cafe-window-settings-2.0.pc

EXTRA_DIST = 					\
	cafe-window-settings-2.0.pc.in

wms_flags = -export_dynamic -avoid-version -module
wmsdir = $(WM_MODULE_DIR)

wms_LTLIBRARIES =			\
	libmarco.la

libmarco_la_SOURCES = 		\
	marco-window-manager.c	\
	marco-window-manager.h

libmarco_la_LDFLAGS = $(wms_flags)
libmarco_la_LIBADD = 		\
	$(CAFECC_LIBS)			\
	libcafe-window-settings.la

-include $(top_srcdir)/git.mk
