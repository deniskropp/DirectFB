## Makefile.am for DirectFB/lib/fusion

SUBDIRS = shm


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fusion.pc


# If the old location isn't cleared, builds of external modules fail
install-exec-local:
	rm -rf $(DESTDIR)$(INTERNALINCLUDEDIR)/core/fusion
	rm -rf $(DESTDIR)$(INTERNALINCLUDEDIR)/fusion


includedir = @INCLUDEDIR@/fusion

include_HEADERS = \
	arena.h			\
	build.h			\
	call.h			\
	conf.h			\
	fusion.h		\
	fusion_internal.h	\
	hash.h			\
	lock.h			\
	object.h		\
	property.h		\
	reactor.h		\
	ref.h			\
	shmalloc.h		\
	types.h			\
	vector.h


lib_LTLIBRARIES = libfusion.la

libfusion_la_SOURCES = \
	arena.c			\
	call.c			\
	conf.c			\
	fusion.c		\
	hash.c			\
	lock.c			\
	object.c		\
	property.c		\
	reactor.c		\
	ref.c			\
	shmalloc.c		\
	vector.c

libfusion_la_LIBADD = \
	shm/libfusion_shm.la	\
	../direct/libdirect.la

libfusion_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)                              	\
	$(DFB_LDFLAGS)


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libfusion_fixed.a

all-local: libfusion_fixed.a

libfusion_fixed.a: .libs/libfusion.a
	rm -f libfusion_fixed.a
	${AR} cru libfusion_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libfusion_fixed.a
	cp -pf libfusion_fixed.a .libs/libfusion.a

.libs/libfusion.a: libfusion.la

else

clean-local:

all-local:

endif


include $(top_srcdir)/rules/nmfile.make
