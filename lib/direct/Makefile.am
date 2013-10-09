## Makefile.am for DirectFB/lib/direct

SUBDIRS = os

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

EXTRA_DIST = \
	kernelmod.c			\
	armasm_memcpy.S			\
	armasm_memcpy.h			\
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

if BUILDARMASM
armasm_sources = armasm_memcpy.S
armasm_header = armasm_memcpy.h
endif

# If the old location isn't cleared, builds of external modules fail
install-exec-local:
	rm -rf $(DESTDIR)$(INTERNALINCLUDEDIR)/direct


includedir = @INCLUDEDIR@/direct

include_HEADERS = \
	$(ppcasm_headers)		\
	$(armasm_headers)		\
	Lists.h				\
	LockWQ.h			\
	Magic.h				\
	Mutex.h				\
	Performer.h			\
	String.h			\
	TLSObject.h			\
	ToString.h			\
	Types++.h			\
	atomic.h			\
	build.h				\
	clock.h				\
	compiler.h			\
	conf.h				\
	debug.h				\
	direct.h			\
	fastlz.h			\
	fifo.h				\
	filesystem.h			\
	flz.h				\
	hash.h				\
	init.h				\
	interface.h			\
	interface_implementation.h	\
	list.h				\
	log.h				\
	log_domain.h			\
	map.h				\
	mem.h				\
	memcpy.h			\
	messages.h			\
	modules.h			\
	perf.h				\
	print.h				\
	processor.h			\
	result.h			\
	serial.h			\
	signals.h			\
	stream.h			\
	system.h			\
	thread.h			\
	trace.h				\
	tree.h				\
	types.h				\
	utf8.h				\
	util.h				\
	uuid.h


lib_LTLIBRARIES = libdirect.la

libdirect_la_SOURCES = \
	$(ppcasm_sources)	\
	$(armasm_sources)	\
	String.cpp		\
	clock.c			\
	conf.c			\
	debug.c			\
	direct.c		\
	direct_result.c		\
	fastlz.c		\
	fifo.c			\
	flz.c			\
	hash.c			\
	init.c			\
	interface.c		\
	list.c			\
	log.c			\
	log_domain.c		\
	map.c			\
	mem.c			\
	memcpy.c		\
	messages.c		\
	modules.c		\
	perf.c			\
	print.c			\
	processor.c		\
	result.c		\
	serial.c		\
	signals.c		\
	stream.c		\
	system.c		\
	trace.c			\
	tree.c			\
	thread.c		\
	utf8.c			\
	util.c			\
	uuid.c

libdirect_la_LIBADD = \
	os/linux/glibc/libdirect_os_linux_glibc.la		\
	-lstdc++

libdirect_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)					\
	$(DFB_LDFLAGS)


MKRESULT = $(top_srcdir)/tools/mkresult.sh

direct_result.c: $(srcdir)/result.h $(MKRESULT)
	@echo '#include <config.h>'    		> $@
	@echo                                   >> $@
	@echo '#include <direct/types.h>'    	>> $@
	@echo                                   >> $@
	@echo                                   >> $@

	@$(MKRESULT) DirectResult DR $< 	>> $@

	@echo                                   >> $@


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


include $(top_srcdir)/rules/nmfile.make
