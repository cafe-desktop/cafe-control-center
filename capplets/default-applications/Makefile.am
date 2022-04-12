SUBDIRS = icons

# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = cafe-default-applications-properties

ui_files = cafe-default-applications-properties.ui

bin_PROGRAMS = cafe-default-applications-properties

BUILT_SOURCES = \
	cafe-default-applications-properties-resources.h \
	cafe-default-applications-properties-resources.c

cafe_default_applications_properties_LDADD = $(CAFECC_CAPPLETS_LIBS)
nodist_cafe_default_applications_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_default_applications_properties_SOURCES = \
	cafe-da-capplet.c cafe-da-capplet.h

include $(top_srcdir)/gla11y.mk

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-default-applications-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cafe-default-applications.pc

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(CAFECC_CAPPLETS_CFLAGS) \
	$(DEFAULT_APPLICATIONS_CAPPLET_CFLAGS) \
	-DAPPLICATIONSDIR=\""$(datadir)/applications"\"

cafe-default-applications-properties-resources.h cafe-default-applications-properties-resources.c: org.cafe.mcc.da.gresource.xml
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name default_applications $<

CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(autostart_DATA) $(BUILT_SOURCES)
EXTRA_DIST = \
	cafe-default-applications.pc.in \
	cafe-default-applications-properties.ui \
	$(Desktop_in_files) \
	org.cafe.mcc.da.gresource.xml

-include $(top_srcdir)/git.mk
