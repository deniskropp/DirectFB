## Makefile.am for lib/++dfb

INCLUDES = \
        -I$(top_builddir)/include       \
        -I$(top_builddir)/src           \
        -I$(top_builddir)/lib           \
        -I$(top_srcdir)/include         \
        -I$(top_srcdir)/include/++dfb   \
        -I$(top_srcdir)/src             \
        -I$(top_srcdir)/lib

lib_LTLIBRARIES = lib++dfb.la

lib__dfb_la_SOURCES = \
	ppdfb.cpp			\
	++dfb_internal.h		\
	idirectfb.cpp			\
	idirectfbdatabuffer.cpp		\
	idirectfbdisplaylayer.cpp	\
	idirectfbeventbuffer.cpp	\
	idirectfbfont.cpp		\
	idirectfbimageprovider.cpp	\
	idirectfbinputdevice.cpp	\
	idirectfbpalette.cpp		\
	idirectfbscreen.cpp		\
	idirectfbsurface.cpp		\
	idirectfbvideoprovider.cpp	\
	idirectfbwindow.cpp


lib__dfb_la_LIBADD = \
	$(DFB_BASE_LIBS)

lib__dfb_la_LDFLAGS = \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
        -release $(LT_RELEASE)	\
        $(DFB_LDFLAGS)


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ++dfb.pc

#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f lib++dfb_fixed.a

all-local: lib++dfb_fixed.a

lib++dfb_fixed.a: .libs/lib++dfb.a
	rm -f lib++dfb_fixed.a
	${AR} cru lib++dfb_fixed.a `find . -name "*.o" | grep -v '.libs'`
	${RANLIB} lib++dfb_fixed.a
	cp -pf lib++dfb_fixed.a .libs/lib++dfb.a

.libs/lib++dfb.a: lib++dfb.la

else

clean-local:

all-local:

endif

include $(top_srcdir)/rules/nmfile.make
include $(top_srcdir)/rules/libs_deps.make
