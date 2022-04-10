# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = window

bin_PROGRAMS = cafe-window-properties

cafe_window_properties_LDADD = $(CAFECC_CAPPLETS_LIBS)

BUILT_SOURCES = cafe-window-properties-resources.h cafe-window-properties-resources.c
nodist_cafe_window_properties_SOURCES = $(BUILT_SOURCES)
cafe_window_properties_SOURCES = \
	cafe-metacity-support.h \
	cafe-metacity-support.c \
	cafe-window-properties.c

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-window-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

cafe-window-properties-resources.h cafe-window-properties-resources.c: org.cafe.mcc.windows.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.mcc.windows.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name window_properties $<

$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

AM_CPPFLAGS =				\
	$(WARN_CFLAGS)			\
	$(CAFECC_CAPPLETS_CFLAGS)	\
	-DUIDIR=\""$(uidir)"\"		\
	-DPIXMAPDIR=\""$(pixmapdir)"\"

CLEANFILES = $(BUILT_SOURCES) $(CAFECC_CAPPLETS_CLEANFILES) $(desktop_DATA)
EXTRA_DIST = $(Desktop_in_files) org.cafe.mcc.windows.gresource.xml window-properties.ui

-include $(top_srcdir)/git.mk
