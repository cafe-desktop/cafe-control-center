# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = keybinding

ui_files = cafe-keybinding-properties.ui

bin_PROGRAMS = cafe-keybinding-properties

cafe_keybinding_properties_LDADD = $(CAFECC_CAPPLETS_LIBS)
BUILT_SOURCES = cafe-keybinding-properties-resources.h cafe-keybinding-properties-resources.c
nodist_cafe_keybinding_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_keybinding_properties_SOURCES = \
	cafe-keybinding-properties.c \
	eggcellrendererkeys.c \
	eggcellrendererkeys.h \
	eggaccelerators.c \
	eggaccelerators.h

include $(top_srcdir)/gla11y.mk

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-keybinding.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

xmldir = $(pkgdatadir)/keybindings
xml_in_files = 00-multimedia-key.xml.in 01-desktop-key.xml.in
xml_DATA = $(xml_in_files:.xml.in=.xml)

SUFFIXES = .xml.in .xml
.xml.in.xml:
	$(AM_V_GEN) GETTEXTDATADIR=$(top_srcdir) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cafe-keybindings.pc

cafe-keybinding-properties-resources.h cafe-keybinding-properties-resources.c: org.cafe.ccc.keybindings.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.ccc.keybindings.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name keybindings $<

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(CAFECC_CAPPLETS_CFLAGS) \
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\""
CLEANFILES = \
	$(BUILT_SOURCES) \
	$(CAFECC_CAPPLETS_CLEANFILES) \
	$(desktop_DATA) \
	$(xml_DATA)

EXTRA_DIST = $(xml_in_files) \
	cafe-keybindings.pc.in \
	$(ui_files) \
	$(Desktop_in_files) \
	org.cafe.ccc.keybindings.gresource.xml \
	ui-a11y.suppr

-include $(top_srcdir)/git.mk
