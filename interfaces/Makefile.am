## Makefile.am for DirectFB/interfaces

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS) $(FREETYPE_CFLAGS)
CXXFLAGS = $(DFB_CXXFLAGS) $(AVIFILE_CFLAGS)

if AVIFILE_PROVIDER
AVIFILE_PROVIDER_LTLIB = libidirectfbvideoprovider_avifile.la
else
AVIFILE_PROVIDER_LTLIB =
endif

interfaces_LTLIBRARIES = \
	libidirectfbfont_ft2.la			\
	libidirectfbimageprovider_png.la	\
	libidirectfbimageprovider_gif.la	\
	libidirectfbimageprovider_jpeg.la	\
	$(AVIFILE_PROVIDER_LTLIB)		\
	libidirectfbvideoprovider_v4l.la

interfacesdir = $(LIBDIR)/interfaces


libidirectfbfont_ft2_la_SOURCES = idirectfbfont_ft2.c

libidirectfbfont_ft2_la_LIBADD = $(FREETYPE_LIBS)

libidirectfbfont_ft2_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbimageprovider_png_la_SOURCES = idirectfbimageprovider_png.c

libidirectfbimageprovider_png_la_LIBADD = $(LIBPNG)

libidirectfbimageprovider_png_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbimageprovider_gif_la_SOURCES = idirectfbimageprovider_gif.c

libidirectfbimageprovider_gif_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbimageprovider_jpeg_la_SOURCES = idirectfbimageprovider_jpeg.c

libidirectfbimageprovider_jpeg_la_LIBADD = $(LIBJPEG)

libidirectfbimageprovider_jpeg_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbvideoprovider_avifile_la_SOURCES = idirectfbvideoprovider_avifile.cpp

libidirectfbvideoprovider_avifile_la_LIBADD = $(AVIFILE_LIBS) -lstdc++

libidirectfbvideoprovider_avifile_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module -disable-static
