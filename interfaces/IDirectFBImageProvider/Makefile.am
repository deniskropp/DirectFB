## Makefile.am for DirectFB/interfaces/IDirectFBImageProvider

idirectfbimageproviderdir = $(MODULEDIR)/interfaces/IDirectFBImageProvider

if GIF_PROVIDER
GIF_PROVIDER_LTLIB = libidirectfbimageprovider_gif.la
else
GIF_PROVIDER_LTLIB = 
endif

if JPEG_PROVIDER
JPEG_PROVIDER_LTLIB = libidirectfbimageprovider_jpeg.la
else
JPEG_PROVIDER_LTLIB = 
endif

if PNG_PROVIDER
PNG_PROVIDER_LTLIB = libidirectfbimageprovider_png.la
else
PNG_PROVIDER_LTLIB = 
endif

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"

CFLAGS = $(DFB_CFLAGS)

idirectfbimageprovider_LTLIBRARIES = \
	$(GIF_PROVIDER_LTLIB)	\
	$(JPEG_PROVIDER_LTLIB)	\
	$(PNG_PROVIDER_LTLIB)

libidirectfbimageprovider_png_la_SOURCES = idirectfbimageprovider_png.c

libidirectfbimageprovider_png_la_LIBADD = $(LIBPNG)

libidirectfbimageprovider_png_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_gif_la_SOURCES = idirectfbimageprovider_gif.c

libidirectfbimageprovider_gif_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_jpeg_la_SOURCES = idirectfbimageprovider_jpeg.c

libidirectfbimageprovider_jpeg_la_LIBADD = $(LIBJPEG)

libidirectfbimageprovider_jpeg_la_LDFLAGS = -avoid-version -module
