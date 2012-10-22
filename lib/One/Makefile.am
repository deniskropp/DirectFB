## Makefile.am for DirectFB/lib/One

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(srcdir)/linux-one/include

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = one.pc


includedir = @GENERIC_INCLUDEDIR@/One

include_HEADERS = \
	One.h	\
	OneQueue.h	\
	OneThread.h	\
	OneTypes.h

lib_LTLIBRARIES = libone.la


libone_la_SOURCES = \
	One.c

libone_la_LIBADD = \
	../direct/libdirect.la

libone_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)                              	\
	$(DFB_LDFLAGS)


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libone_fixed.a

all-local: libone_fixed.a

libone_fixed.a: .libs/libone.a
	rm -f libone_fixed.a
	${AR} cru libone_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libone_fixed.a
	cp -pf libone_fixed.a .libs/libone.a

.libs/libone.a: libone.la

else

clean-local:

all-local:

endif


include $(top_srcdir)/rules/nmfile.make
