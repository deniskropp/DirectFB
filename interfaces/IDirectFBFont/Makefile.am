## Makefile.am for DirectFB/interfaces/IDirectFBFont

idirectfbfontdir = $(LIBDIR)/interfaces/IDirectFBFont

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"

CFLAGS = $(DFB_CFLAGS) $(FREETYPE_CFLAGS)

idirectfbfont_LTLIBRARIES = \
	libidirectfbfont_default.la	\
	libidirectfbfont_ft2.la


libidirectfbfont_default_la_SOURCES = idirectfbfont_default.c

libidirectfbfont_default_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbfont_ft2_la_SOURCES = idirectfbfont_ft2.c

libidirectfbfont_ft2_la_LIBADD = $(FREETYPE_LIBS)

libidirectfbfont_ft2_la_LDFLAGS = -avoid-version -module -disable-static
