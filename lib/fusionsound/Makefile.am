## Makefile.am for lib/fusionsound

SUBDIRS = core media misc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusionsound


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fusionsound.pc fusionsound-internal.pc

includedir = @GENERIC_INCLUDEDIR@/fusionsound

include_HEADERS = \
	fusionsound.h \
	fusionsound_limits.h \
	fusionsound_version.h


lib_LTLIBRARIES = libfusionsound.la

libfusionsound_la_SOURCES = \
	fusionsound.c			\
	ifusionsound.c			\
	ifusionsound.h			\
	ifusionsoundbuffer.c	\
	ifusionsoundbuffer.h	\
	ifusionsoundplayback.c	\
	ifusionsoundplayback.h	\
	ifusionsoundstream.c	\
	ifusionsoundstream.h

libfusionsound_la_LIBADD = \
	../direct/libdirect.la \
	../fusion/libfusion.la \
	core/libfusionsoundcore.la \
	misc/libfusionsoundmisc.la \
	media/libfusionsoundmedia.la

libfusionsound_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	$(DFB_LDFLAGS)

#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libfusionsound_fixed.a

all-local: libfusionsound_fixed.a

libfusionsound_fixed.a: .libs/libfusionsound.a
	rm -f libfusionsound_fixed.a
	${AR} cru libfusionsound_fixed.a `find -name '*.o' | grep -v '.libs'`
	${RANLIB} libfusionsound_fixed.a
	cp -af libfusionsound_fixed.a .libs/libfusionsound.a

.libs/libfusionsound.a: libfusionsound.la

else

clean-local:

all-local:

endif

include $(top_srcdir)/rules/nmfile.make
