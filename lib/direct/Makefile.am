## Makefile.am for DirectFB/lib/direct

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


noinst_PROGRAMS = dtest

dtest_SOURCES = dtest.c
dtest_LDADD   = libdirect.la


EXTRA_DIST = \
	ppcasm_memcpy.S			\
	ppcasm_memcpy_cachable.S	\
	ppcasm_memcpy.h			\
	ppc_asm.h

if BUILDPPCASM
if HAVE_LINUX
ppcasm_sources = ppcasm_memcpy.S ppcasm_memcpy_cachable.S
else
ppcasm_sources = ppcasm_memcpy.S
endif

ppcasm_headers = ppcasm_memcpy.h ppc_asm.h
endif


internalincludedir = $(INTERNALINCLUDEDIR)/direct

internalinclude_HEADERS = \
	$(ppcasm_headers)	\
	clock.h			\
	conf.h			\
	cpu_accel.h		\
	debug.h			\
	list.h			\
	mem.h			\
	memcpy.h		\
	messages.h		\
	signals.h		\
	system.h		\
	thread.h		\
	trace.h			\
	types.h


lib_LTLIBRARIES = libdirect.la

libdirect_la_SOURCES = \
	$(ppcasm_sources)	\
	clock.c			\
	conf.c			\
	cpu_accel.c		\
	debug.c			\
	list.c			\
	mem.c			\
	memcpy.c		\
	messages.c		\
	signals.c		\
	system.c		\
	trace.c			\
	thread.c

libdirect_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)					\
	$(DFB_LDFLAGS)


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libdirect_fixed.a

all-local: libdirect_fixed.a

libdirect_fixed.a: .libs/libdirect.a
	rm -f libdirect_fixed.a
	${AR} cru libdirect_fixed.a `find -name "*.o"`
	${RANLIB} libdirect_fixed.a
	cp -af libdirect_fixed.a .libs/libdirect.a

.libs/libdirect.a: libdirect.la

else

clean-local:

all-local:

endif
