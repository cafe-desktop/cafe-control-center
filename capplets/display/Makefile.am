# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = display

BUILT_SOURCES = \
	foo-marshal.h \
	foo-marshal.c \
	cafe-display-properties-resources.h \
	cafe-display-properties-resources.c

ui_files = display-capplet.ui

include $(top_srcdir)/gla11y.mk

bin_PROGRAMS = cafe-display-properties

sbin_PROGRAMS = cafe-display-properties-install-systemwide

nodist_cafe_display_properties_SOURCES = \
	$(BUILT_SOURCES)
cafe_display_properties_SOURCES =	\
	xrandr-capplet.c		\
	scrollarea.c			\
	scrollarea.h

cafe_display_properties_LDFLAGS = -export-dynamic
cafe_display_properties_LDADD = \
	$(top_builddir)/capplets/common/libcommon.la \
	$(DISPLAY_CAPPLET_LIBS)

cafe_display_properties_install_systemwide_SOURCES =	\
	cafe-display-properties-install-systemwide.c

cafe_display_properties_install_systemwide_LDADD =	\
	$(GLIB_LIBS)

polkit_policydir = $(datadir)/polkit-1/actions
dist_polkit_policy_DATA =					\
	org.cafe.randr.policy

SUFFIXES = .policy.in .policy
.policy.in.policy:
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

desktopdir = $(datadir)/applications
Desktop_in_files = cafe-display-properties.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

AM_CPPFLAGS =					\
	$(WARN_CFLAGS)				\
	$(DISPLAY_CAPPLET_CFLAGS)		\
	$(CAFECC_CAPPLETS_CFLAGS)		\
	-DSBINDIR="\"$(sbindir)\""		\
	-DCAFELOCALEDIR="\"$(datadir)/locale\""	\
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\""

foo-marshal.c: foo-marshal.list
	@GLIB_GENMARSHAL@ --prefix=foo_marshal $< --header > $@
foo-marshal.h: foo-marshal.list
	@GLIB_GENMARSHAL@ --prefix=foo_marshal $< --body --prototypes > $@
cafe-display-properties-resources.h cafe-display-properties-resources.c: org.cafe.mcc.display.gresource.xml
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name display $<

CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(BUILT_SOURCES)

EXTRA_DIST = $(ui_files) foo-marshal.list $(Desktop_in_files) org.cafe.mcc.display.gresource.xml org.cafe.randr.policy.in ui-a11y.suppr
DISTCLEANFILES = org.cafe.randr.policy

-include $(top_srcdir)/git.mk
