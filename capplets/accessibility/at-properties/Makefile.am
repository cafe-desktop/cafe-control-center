bin_PROGRAMS = cafe-at-properties

ui_files = at-enable-dialog.ui

BUILT_SOURCES = \
	cafe-at-properties-resources.c \
	cafe-at-properties-resources.h
nodist_cafe_at_properties_SOURCES= $(BUILT_SOURCES)
cafe_at_properties_LDADD = $(AT_CAPPLET_LIBS) $(CAFECC_CAPPLETS_LIBS) $(top_builddir)/capplets/common/libcommon.la
cafe_at_properties_SOURCES = \
	dm-util.h	\
	dm-util.c	\
	main.c
cafe_at_properties_LDFLAGS = -export-dynamic

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-at-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

pixmapdir  = $(pkgdatadir)/pixmaps
pixmap_DATA =					\
	at-startup.png				\
	at-support.png

include $(top_srcdir)/gla11y.mk

AM_CPPFLAGS =					\
	$(WARN_CFLAGS)				\
	$(AT_CAPPLET_CFLAGS)			\
	$(CAFECC_CAPPLETS_CFLAGS)		\
	-DPIXMAPDIR=\""$(pixmapdir)"\"		\
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\""
CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = \
	$(ui_files) \
	$(pixmap_DATA) \
	$(Desktop_in_files) \
	org.cafe.ccc.accessibility.at.gresource.xml \
	ui-a11y.suppr

# Generate resources
cafe-at-properties-resources.h cafe-at-properties-resources.c: org.cafe.ccc.accessibility.at.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.ccc.accessibility.at.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name accessibility $<

-include $(top_srcdir)/git.mk
