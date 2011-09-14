## Makefile.am for DirectFB/interfaces/IDirectFBFont

idirectfbfontdir = $(MODULEDIR)/interfaces/IDirectFBFont

if FREETYPE_PROVIDER
FREETYPE_PROVIDER_LTLIB = libidirectfbfont_ft2.la
else
FREETYPE_PROVIDER_LTLIB =
endif

if LINOTYPE_PROVIDER
LINOTYPE_PROVIDER_LTLIB = libidirectfbfont_lino.la
else
LINOTYPE_PROVIDER_LTLIB =
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/src	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

AM_CFLAGS = $(FREETYPE_CFLAGS) $(LINOTYPE_CFLAGS)

idirectfbfont_LTLIBRARIES = \
	$(FREETYPE_PROVIDER_LTLIB)	\
	$(LINOTYPE_PROVIDER_LTLIB)	\
	libidirectfbfont_dgiff.la

if BUILD_STATIC
idirectfbfont_DATA = $(idirectfbfont_LTLIBRARIES:.la=.o)
endif


libidirectfbfont_dgiff_la_SOURCES = idirectfbfont_dgiff.c

libidirectfbfont_dgiff_la_LDFLAGS = -avoid-version -module

libidirectfbfont_dgiff_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


libidirectfbfont_ft2_la_SOURCES = idirectfbfont_ft2.c

libidirectfbfont_ft2_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(FREETYPE_LIBS) \
	$(LIBM)

libidirectfbfont_ft2_la_LDFLAGS = -avoid-version -module


libidirectfbfont_lino_la_SOURCES = idirectfbfont_lino.c

libidirectfbfont_lino_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LINOTYPE_LIBS)

libidirectfbfont_lino_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make
