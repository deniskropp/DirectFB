## Makefile.am for DirectFB/lib/fusion

if ENABLE_MULTI
SUBDIRS = shmalloc
endif

if ENABLE_MULTI
SHMALLOC_LIB = shmalloc/libfusion_shmalloc.la
else
SHMALLOC_LIB =
endif


INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


internalincludedir = $(INTERNALINCLUDEDIR)/fusion

internalinclude_HEADERS = \
	arena.h		\
	build.h		\
	call.h		\
	conf.h		\
	fusion.h	\
	lock.h		\
	object.h	\
	property.h	\
	reactor.h	\
	ref.h		\
	shmalloc.h	\
	types.h		\
	vector.h


lib_LTLIBRARIES = libfusion.la

libfusion_la_SOURCES = \
	arena.c			\
	call.c			\
	conf.c			\
	fusion.c		\
	fusion_internal.h	\
	fusion_shmalloc.c	\
	lock.c			\
	object.c		\
	property.c		\
	reactor.c		\
	ref.c			\
	vector.c

libfusion_la_LIBADD = \
	$(SHMALLOC_LIB)		\
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
	${AR} cru libfusion_fixed.a `find -name "*.o"`
	${RANLIB} libfusion_fixed.a
	cp -af libfusion_fixed.a .libs/libfusion.a

.libs/libfusion.a: libfusion.la

else

clean-local:

all-local:

endif
