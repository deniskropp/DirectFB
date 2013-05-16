## Makefile.am for lib/fusiondale

SUBDIRS = coma core messenger misc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusiondale	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusiondale

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = fusiondale.pc

includedir = @GENERIC_INCLUDEDIR@/fusiondale

include_HEADERS = \
	fusiondale.h \
	fusiondale_util.h \
	fusiondale_version.h

lib_LTLIBRARIES = libfusiondale.la

libfusiondale_la_SOURCES = \
	fusiondale.c			\
	ifusiondale.c			\
	ifusiondale.h

libfusiondale_la_LIBADD = \
	../direct/libdirect.la \
	../fusion/libfusion.la \
	coma/libfusiondalecoma.la \
	core/libfusiondalecore.la \
	messenger/libfusiondalemessenger.la \
	misc/libfusiondalemisc.la

libfusiondale_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	$(DFB_LDFLAGS)

#
## and now rebuild the static version with the *correct* object files
#
if BUILD_STATIC

clean-local:
	rm -f libfusiondale_fixed.a

all-local: libfusiondale_fixed.a

libfusiondale_fixed.a: .libs/libfusiondale.a
	rm -f libfusiondale_fixed.a
	${AR} cru libfusiondale_fixed.a `find -name '*.o' | grep -v '.libs'`
	${RANLIB} libfusiondale_fixed.a
	cp -af libfusiondale_fixed.a .libs/libfusiondale.a

.libs/libfusiondale.a: libfusiondale.la

else

clean-local:

all-local:

endif
