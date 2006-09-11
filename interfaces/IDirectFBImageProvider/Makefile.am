## Makefile.am for DirectFB/interfaces/IDirectFBImageProvider

SUBDIRS = mpeg2

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
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_dfiff.la	\
	libidirectfbimageprovider_mpeg2.la	\
	$(GIF_PROVIDER_LTLIB)			\
	$(JPEG_PROVIDER_LTLIB)			\
	$(PNG_PROVIDER_LTLIB)

if BUILD_STATIC
idirectfbimageprovider_DATA = $(idirectfbimageprovider_LTLIBRARIES:.la=.o)
endif


libidirectfbimageprovider_png_la_SOURCES = idirectfbimageprovider_png.c

libidirectfbimageprovider_png_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBPNG)

libidirectfbimageprovider_png_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_mpeg2_la_SOURCES = idirectfbimageprovider_mpeg2.c

libidirectfbimageprovider_mpeg2_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la \
	mpeg2/libmpeg2decode.la

libidirectfbimageprovider_mpeg2_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_dfiff_la_SOURCES = idirectfbimageprovider_dfiff.c

libidirectfbimageprovider_dfiff_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la

libidirectfbimageprovider_dfiff_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_gif_la_SOURCES = idirectfbimageprovider_gif.c

libidirectfbimageprovider_gif_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbimageprovider_gif_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_jpeg_la_SOURCES = idirectfbimageprovider_jpeg.c

libidirectfbimageprovider_jpeg_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBJPEG)

libidirectfbimageprovider_jpeg_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make
