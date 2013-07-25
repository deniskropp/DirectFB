## Makefile.am for lib/divine

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/src		\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/divine	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/lib             \
	-I$(top_srcdir)/lib/divine

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"	\
	-DSYSCONFDIR=\"@sysconfdir@\" \
	-DFLUXED_ARGS_BYTES=@FLUXED_ARGS_BYTES@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = divine.pc

includedir = @GENERIC_INCLUDEDIR@/divine

include_HEADERS = \
	divine.h

lib_LTLIBRARIES = libdivine.la

libdivine_la_SOURCES = \
	divine.c		\
	idivine.c

libdivine_la_LIBADD = \
	$(DFB_BASE_LIBS)

libdivine_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)	\
	$(DFB_LDFLAGS)


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libdivine_fixed.a

all-local: libdivine_fixed.a

libdivine_fixed.a: .libs/libdivine.a
	rm -f libdivine_fixed.a
	${AR} cru libdivine_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libdivine_fixed.a
	cp -pf libdivine_fixed.a .libs/libdivine.a

.libs/libdivine.a: libdivine.la

else

clean-local:

all-local:

endif


include $(top_srcdir)/rules/nmfile.make
include $(top_srcdir)/rules/libs_deps.make
