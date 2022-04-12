# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = mouse

ui_files = cafe-mouse-properties.ui

bin_PROGRAMS = cafe-mouse-properties

cafe_mouse_properties_LDADD = $(CAFECC_CAPPLETS_LIBS)
BUILT_SOURCES = cafe-mouse-properties-resources.h cafe-mouse-properties-resources.c
nodist_cafe_mouse_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_mouse_properties_SOURCES =	\
	cafe-mouse-properties.c	\
	msd-input-helper.c	\
	msd-input-helper.h

include $(top_srcdir)/gla11y.mk

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-settings-mouse.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

cafe-mouse-properties-resources.h cafe-mouse-properties-resources.c: org.cafe.mcc.mouse.gresource.xml
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name mouse $<

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(CAFECC_CAPPLETS_CFLAGS) \
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\""
CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = \
	$(ui_files) \
	double-click-on.svg \
	double-click-off.svg \
	double-click-maybe.svg \
	$(Desktop_in_files) \
	org.cafe.mcc.mouse.gresource.xml \
	ui-a11y.suppr

-include $(top_srcdir)/git.mk
