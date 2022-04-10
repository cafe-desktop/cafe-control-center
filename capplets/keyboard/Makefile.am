# This is used in MATECC_CAPPLETS_CFLAGS
cappletname = keyboard

ui_files = cafe-keyboard-properties-a11y-notifications.ui \
           cafe-keyboard-properties-dialog.ui \
           cafe-keyboard-properties-layout-chooser.ui \
           cafe-keyboard-properties-model-chooser.ui \
           cafe-keyboard-properties-options-dialog.ui


bin_PROGRAMS = cafe-keyboard-properties

BUILT_SOURCES = cafe-keyboard-properties-resources.h cafe-keyboard-properties-resources.c
nodist_cafe_keyboard_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_keyboard_properties_SOURCES = \
	cafe-keyboard-properties.c \
	cafe-keyboard-properties-a11y.c \
	cafe-keyboard-properties-a11y.h \
	cafe-keyboard-properties-xkb.c \
	cafe-keyboard-properties-xkbmc.c \
	cafe-keyboard-properties-xkblt.c \
	cafe-keyboard-properties-xkbltadd.c \
	cafe-keyboard-properties-xkbot.c \
	cafe-keyboard-properties-xkbpv.c \
	cafe-keyboard-properties-xkb.h

cafe_keyboard_properties_LDADD = $(MATECC_CAPPLETS_LIBS) $(LIBMATEKBDUI_LIBS)

include $(top_srcdir)/gla11y.mk

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

cafe-keyboard-properties-resources.h cafe-keyboard-properties-resources.c: org.cafe.mcc.keyboard.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.mcc.keyboard.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name keyboard $<

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(MATECC_CAPPLETS_CFLAGS) \
	$(LIBMATEKBDUI_CFLAGS) \
	-DMATECC_DATA_DIR="\"$(pkgdatadir)\""
CLEANFILES = $(MATECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = \
	$(ui_files) \
	$(Desktop_in_files) \
	org.cafe.mcc.keyboard.gresource.xml \
	ui-a11y.suppr

-include $(top_srcdir)/git.mk
