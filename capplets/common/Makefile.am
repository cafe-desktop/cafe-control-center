EXTRA_DIST =

AM_CPPFLAGS = \
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\""				\
	-DCAFELOCALEDIR="\"$(datadir)/locale\""				\
	-DGTK_ENGINE_DIR="\"$(GTK_ENGINE_DIR)\"" 			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DINSTALL_PREFIX=\"$(prefix)\"					\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libwindow-settings				\
	-DPIXMAP_DIR=\""$(datadir)/cafe-control-center/pixmaps"\"	\
	$(WARN_CFLAGS)							\
	$(CAPPLET_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(CAFE_DESKTOP_CFLAGS)						\
	$(MARCO_CFLAGS)							\
	$(GSD_DBUS_CFLAGS)						\
	$(GIO_CFLAGS)							\
	$(DCONF_CFLAGS)


noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	activate-settings-daemon.c	\
	activate-settings-daemon.h	\
	capplet-util.c			\
	capplet-util.h			\
	dconf-util.c			\
	dconf-util.h			\
	file-transfer-dialog.c		\
	file-transfer-dialog.h		\
	cafe-theme-apply.c		\
	cafe-theme-apply.h 		\
	cafe-theme-info.c		\
	cafe-theme-info.h		\
	gtkrc-utils.c			\
	gtkrc-utils.h			\
	theme-thumbnail.c		\
	theme-thumbnail.h		\
	wm-common.c			\
	wm-common.h

libcommon_la_LIBADD =							\
	$(top_builddir)/libwindow-settings/libcafe-window-settings.la  \
	$(MARCO_LIBS)						\
	$(DBUS_LIBS)							\
	$(CAFE_DESKTOP_LIBS)						\
	$(GIO_LIBS)									\
	$(DCONF_LIBS)

cafe_theme_test_SOURCES = \
	cafe-theme-test.c

cafe_theme_test_LDADD = 						\
	libcommon.la							\
	$(CAFECC_CAPPLETS_LIBS)						\
	$(CAFECC_LIBS)

noinst_PROGRAMS = \
	cafe-theme-test

-include $(top_srcdir)/git.mk
