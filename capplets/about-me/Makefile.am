# This is used in CAFECC_CAPPLETS_CFLAGS
cappletname = about-me

ui_files = cafe-about-me-dialog.ui cafe-about-me-password.ui cafe-about-me-fingerprint.ui
Desktop_in_files = cafe-about-me.desktop.in

BUILT_SOURCES = marshal.c marshal.h cafe-about-me-resources.h cafe-about-me-resources.c
nodist_cafe_about_me_SOURCES = \
	$(BUILT_SOURCES)
cafe_about_me_SOURCES =	 	\
	cafe-about-me-password.c	\
	cafe-about-me-password.h	\
	e-image-chooser.c		\
	e-image-chooser.h		\
	cafe-about-me-fingerprint.c	\
	cafe-about-me-fingerprint.h	\
	fingerprint-strings.h		\
	cafe-about-me.c

marshal.h: fprintd-marshal.list
	@GLIB_GENMARSHAL@ --prefix=fprintd_marshal $< --header > $@
marshal.c: fprintd-marshal.list
	@GLIB_GENMARSHAL@ --prefix=fprintd_marshal $< --body --prototypes > $@
cafe-about-me-resources.h cafe-about-me-resources.c: org.cafe.ccc.am.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.ccc.am.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name about_me $<

bin_PROGRAMS = cafe-about-me

cafe_about_me_LDADD = $(CAFECC_CAPPLETS_LIBS)
if HAVE_ACCOUNTSSERVICE
cafe_about_me_LDADD += $(ACCOUNTSSERVICE_LIBS)
endif
cafe_about_me_LDFLAGS = -export-dynamic

desktopdir = $(datadir)/applications
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(Desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword= --keyword=Name --keyword=Comment --keyword=Keywords --template $< -d $(top_srcdir)/po -o $@

include $(top_srcdir)/gla11y.mk

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(CAFECC_CAPPLETS_CFLAGS) \
	$(LIBEBOOK_CFLAGS) \
	-DDATADIR="\"$(datadir)\"" \
	-DCAFECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DCAFECC_PIXMAP_DIR="\"$(pkgdatadir)/pixmaps\"" \
	-DCAFELOCALEDIR="\"$(datadir)/locale\""

if HAVE_ACCOUNTSSERVICE
AM_CPPFLAGS += \
	$(ACCOUNTSSERVICE_CFLAGS)
endif

CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = \
	$(ui_files) \
	fprintd-marshal.list \
	$(Desktop_in_files) \
	org.cafe.ccc.am.gresource.xml \
	ui-a11y.suppr

-include $(top_srcdir)/git.mk
