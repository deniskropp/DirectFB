## Makefile.am for SaWMan/src

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/src		\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/sawman	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/lib             \
	-I$(top_srcdir)/lib/sawman

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"	\
	-DSYSCONFDIR=\"@sysconfdir@\" \
	-DFLUXED_ARGS_BYTES=@FLUXED_ARGS_BYTES@


MKNAMES = $(top_srcdir)/tools/mknames.sh
INCP = $(srcdir)

DISTCLEANFILES = sawman_strings.h

sawman_strings.h: sawman.h $(MKNAMES) Makefile
	@echo '#ifndef __SAWMAN__SAWMAN_STRINGS_H__' > $@
	@echo '#define __SAWMAN__SAWMAN_STRINGS_H__' >> $@
	@echo '#include <sawman.h>' >> $@

	@$(MKNAMES) SaWManProcessFlags SWMPF NONE SaWManProcessFlags flag $(INCP)/sawman.h \
	   | grep -v SWMPF_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = sawman.pc

includedir = @GENERIC_INCLUDEDIR@/sawman

include_HEADERS = \
	Debug.h			\
	sawman.h		\
	sawman_strings.h	\
	sawman_types.h

lib_LTLIBRARIES = libsawman.la

libsawman_la_SOURCES = \
	Debug.cpp		\
	SaWMan.cpp		\
	SaWMan.h		\
	SaWMan_includes.h	\
	SaWMan_real.cpp		\
	SaWManManager.cpp		\
	SaWManManager.h			\
	SaWManManager_includes.h	\
	SaWManManager_real.cpp		\
	SaWManProcess.cpp		\
	SaWManProcess.h			\
	SaWManProcess_includes.h	\
	SaWManProcess_real.cpp		\
	isawman.c		\
	isawman.h		\
	isawmanmanager.c	\
	isawmanmanager.h	\
	region.c		\
	region.h		\
	sawman_core.c 		\
	sawman_internal.h 	\
	sawman_config.c 	\
	sawman_config.h		\
	sawman_draw.c 		\
	sawman_draw.h 		\
	sawman_updates.c	\
	sawman_updates.h	\
	sawman_window.c		\
	sawman_window.h

libsawman_la_LIBADD = \
	$(DFB_BASE_LIBS)

libsawman_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)	\
	$(DFB_LDFLAGS)


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libsawman_fixed.a

all-local: libsawman_fixed.a

libsawman_fixed.a: .libs/libsawman.a
	rm -f libsawman_fixed.a
	${AR} cru libsawman_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libsawman_fixed.a
	cp -pf libsawman_fixed.a .libs/libsawman.a

.libs/libsawman.a: libsawman.la

else

clean-local:

all-local:

endif


distclean-local:
	rm -f SaWMan.cpp
	rm -f SaWMan.h
	rm -f SaWManManager.cpp
	rm -f SaWManManager.h
	rm -f SaWManProcess.cpp
	rm -f SaWManProcess.h


$(srcdir)/Debug.cpp: sawman_strings.h


include $(top_srcdir)/rules/flux_comp_sawman.make
include $(top_srcdir)/rules/nmfile.make
include $(top_srcdir)/rules/libs_deps.make
