bin_PROGRAMS = cafe-typing-monitor

cafe_typing_monitor_SOURCES =			\
	main.c					\
	drwright.c				\
	drwright.h				\
	drw-break-window.c			\
	drw-break-window.h			\
	drw-monitor.c				\
	drw-monitor.h				\
	drw-utils.c				\
	drw-utils.h				\
	drw-selection.c				\
	drw-selection.h				\
	drw-timer.c				\
	drw-timer.h

cafe_typing_monitor_CPPFLAGS = \
	-DCAFELOCALEDIR="\"$(datadir)/locale\""	\
	-DIMAGEDIR=\"$(pkgdatadir)/pixmaps\"		\
	$(AM_CPPFLAGS)
cafe_typing_monitor_CFLAGS = \
	@TYPING_CFLAGS@	\
	@CAFE_DESKTOP_CFLAGS@ \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

cafe_typing_monitor_LDADD = @TYPING_LIBS@ @CAFE_DESKTOP_LIBS@ @SCREENSAVER_LIBS@

if HAVE_LIBKANBERRA_CTK
  cafe_typing_monitor_CFLAGS += -DHAVE_KANBERRA_CTK @LIBKANBERRA_CTK_CFLAGS@ 
  cafe_typing_monitor_LDADD += @LIBKANBERRA_CTK_LIBS@
endif

if HAVE_APP_INDICATOR
  cafe_typing_monitor_CFLAGS += -DHAVE_APP_INDICATOR @APP_INDICATOR_CFLAGS@
  cafe_typing_monitor_LDADD += @APP_INDICATOR_LIBS@
endif

imagedir = $(pkgdatadir)/pixmaps
dist_image_DATA = bar.png bar-red.png bar-green.png bar-disabled.png ocean-stripes.png

# Themeable application icon
icondir = $(datadir)/icons/hicolor/48x48/apps
dist_icon_DATA = cafe-typing-monitor.png
svgicondir = $(datadir)/icons/hicolor/scalable/apps
dist_svgicon_DATA = cafe-typing-monitor.svg
ctk_update_icon_cache = ctk-update-icon-cache -f -t $(datadir)/icons/hicolor
install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Ctk icon cache."; \
		$(ctk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(ctk_update_icon_cache)"; \
	fi

-include $(top_srcdir)/git.mk
