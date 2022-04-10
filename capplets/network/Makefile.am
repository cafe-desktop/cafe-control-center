# This is used in MATECC_CAPPLETS_CFLAGS
cappletname = network

ui_files = cafe-network-properties.ui

bin_PROGRAMS = cafe-network-properties

BUILT_SOURCES = cafe-network-properties-resources.h cafe-network-properties-resources.c
nodist_cafe_network_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_network_properties_SOURCES = cafe-network-properties.c
cafe_network_properties_LDADD = $(MATECC_CAPPLETS_LIBS)

include $(top_srcdir)/gla11y.mk

desktopdir = $(datadir)/applications
desktop_in_files = cafe-network-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

cafe-network-properties-resources.h cafe-network-properties-resources.c: org.cafe.mcc.network.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.mcc.network.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name network $<

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(MATECC_CAPPLETS_CFLAGS)

CLEANFILES = $(MATECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(ui_files) $(desktop_in_files) org.cafe.mcc.network.gresource.xml ui-a11y.suppr

-include $(top_srcdir)/git.mk
