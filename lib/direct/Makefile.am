## Makefile.am for DirectFB/lib/direct

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = direct.pc

noinst_PROGRAMS = dtest

dtest_SOURCES = dtest.c
dtest_LDADD   = libdirect.la
dtest_CFLAGS  = $(CPPFLAGS)


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


# If the old location isn't cleared, builds of external modules fail
install-exec-local:
	rm -rf $(DESTDIR)$(INTERNALINCLUDEDIR)/direct


includedir = @INCLUDEDIR@/direct

include_HEADERS = \
	$(ppcasm_headers)		\
	build.h				\
	clock.h				\
	conf.h				\
	cpu_accel.h			\
	debug.h				\
	direct.h			\
	hash.h				\
	interface.h			\
	interface_implementation.h	\
	list.h				\
	log.h				\
	mem.h				\
	memcpy.h			\
	messages.h			\
	modules.h			\
	serial.h			\
	signals.h			\
	stream.h			\
	system.h			\
	thread.h			\
	trace.h				\
	tree.h				\
	types.h				\
	utf8.h				\
	util.h


lib_LTLIBRARIES = libdirect.la

libdirect_la_SOURCES = \
	$(ppcasm_sources)	\
	clock.c			\
	conf.c			\
	cpu_accel.c		\
	debug.c			\
	direct.c		\
	hash.c			\
	interface.c		\
	list.c			\
	log.c			\
	mem.c			\
	memcpy.c		\
	messages.c		\
	modules.c		\
	signals.c		\
	stream.c		\
	system.c		\
	trace.c			\
	tree.c			\
	thread.c		\
	utf8.c			\
	util.c

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
	${AR} cru libdirect_fixed.a `find . -name "*.o" | grep -v '.libs' | grep -v dtest`
	${RANLIB} libdirect_fixed.a
	cp -pf libdirect_fixed.a .libs/libdirect.a

.libs/libdirect.a: libdirect.la

else

clean-local:

all-local:

endif
