## Makefile.am for DirectFB/interfaces/IDirectFBImageProvider

idirectfbimageproviderdir = $(LIBDIR)/interfaces/IDirectFBImageProvider

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"

CFLAGS = $(DFB_CFLAGS)

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_png.la	\
	libidirectfbimageprovider_gif.la	\
	libidirectfbimageprovider_jpeg.la


libidirectfbimageprovider_png_la_SOURCES = idirectfbimageprovider_png.c

libidirectfbimageprovider_png_la_LIBADD = $(LIBPNG)

libidirectfbimageprovider_png_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbimageprovider_gif_la_SOURCES = idirectfbimageprovider_gif.c

libidirectfbimageprovider_gif_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbimageprovider_jpeg_la_SOURCES = idirectfbimageprovider_jpeg.c

libidirectfbimageprovider_jpeg_la_LIBADD = $(LIBJPEG)

libidirectfbimageprovider_jpeg_la_LDFLAGS = -avoid-version -module -disable-static
