## Makefile.am for DirectFB/src


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SUBDIRS = 
else
NON_PURE_VOODOO_SUBDIRS = \
	core display windows
endif

SUBDIRS = \
	$(NON_PURE_VOODOO_SUBDIRS)	\
	gfx input media misc


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"


internalincludedir = $(INTERNALINCLUDEDIR)

internalinclude_HEADERS = \
	idirectfb.h		\
	init.h


lib_LTLIBRARIES = libdirectfb.la

if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SOURCES = 
else
NON_PURE_VOODOO_SOURCES = \
	idirectfb.c
endif

libdirectfb_la_SOURCES = 		\
	$(NON_PURE_VOODOO_SOURCES)	\
	directfb.c			\
	directfb_result.c		\
	dummy.cpp			\
	init.c

libdirectfb_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)					\
	$(DFB_LDFLAGS)


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_LIBS = 
else
NON_PURE_VOODOO_LIBS = \
	core/libdirectfb_core.la	\
	display/libdirectfb_display.la	\
	windows/libdirectfb_windows.la
endif

if DIRECTFB_BUILD_ONE
ONE_LIB = \
	../lib/One/libone.la
else
ONE_LIB =
endif

if DIRECTFB_BUILD_VOODOO
VOODOO_LIB = \
	../lib/voodoo/libvoodoo.la
else
VOODOO_LIB = 
endif

libdirectfb_la_LIBADD = \
	$(NON_PURE_VOODOO_LIBS)		\
	media/libdirectfb_media.la	\
	input/libdirectfb_input.la	\
	misc/libdirectfb_misc.la	\
	gfx/libdirectfb_gfx.la		\
	../lib/direct/libdirect.la	\
	../lib/fusion/libfusion.la	\
	$(ONE_LIB)			\
	$(VOODOO_LIB)


MKRESULT = $(top_srcdir)/tools/mkresult.sh

directfb_result.c: $(top_srcdir)/include/dfb_types.h $(MKRESULT)
	@echo '#include <config.h>'    		> $@
	@echo                                   >> $@
	@echo '#include <directfb.h>'    	>> $@
	@echo                                   >> $@
	@echo                                   >> $@

	@$(MKRESULT) DFBResult DFB $< 	>> $@

	@echo                                   >> $@


#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libdirectfb_fixed.a

all-local: libdirectfb_fixed.a

libdirectfb_fixed.a: .libs/libdirectfb.a
	rm -f libdirectfb_fixed.a
	${AR} cru libdirectfb_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} libdirectfb_fixed.a
	cp -pf libdirectfb_fixed.a .libs/libdirectfb.a

.libs/libdirectfb.a: libdirectfb.la

else

clean-local:

all-local:

endif


include $(top_srcdir)/rules/nmfile.make
