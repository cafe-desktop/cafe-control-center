cappletname = time-admin

bin_PROGRAMS = cafe-time-admin

BUILT_SOURCES = cafe-time-admin-resources.h cafe-time-admin-resources.c
nodist_cafe_time_admin_SOURCES = $(BUILT_SOURCES)
cafe_time_admin_SOURCES =		\
	main.c       time-map.c  time-share.c  time-tool.c  time-zone.c \
	time-map.h  time-share.h  time-tool.h  time-zone.h

cafe-time-admin-resources.h cafe-time-admin-resources.c: org.cafe.mcc.ta.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.mcc.ta.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name time_admin $<

cafe_time_admin_LDADD = $(CAFECC_CAPPLETS_LIBS)
cafe_time_admin_LDFLAGS = -export-dynamic

AM_CPPFLAGS =				\
	$(WARN_CFLAGS)			\
	$(CAFECC_CAPPLETS_CFLAGS)	\
	-DTIMPZONEDIR="\"$(datadir)/cafe-time-admin/map/\""


CLEANFILES = $(CAFECC_CAPPLETS_CLEANFILES) $(BUILT_SOURCES)

EXTRA_DIST = time-admin.ui time-zones.h org.cafe.mcc.ta.gresource.xml

-include $(top_srcdir)/git.mk
