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

if IMLIB2
IMLIB2_PROVIDER = libidirectfbimageprovider_imlib2.la
else
IMLIB2_PROVIDER = 
endif

if PNM
PNM_PROVIDER = libidirectfbimageprovider_pnm.la
else
PNM_PROVIDER =
endif

if SVG
SVG_PROVIDER = libidirectfbimageprovider_svg.la
else
SVG_PROVIDER =
endif

if BMP
BMP_PROVIDER = libidirectfbimageprovider_bmp.la
else
BMP_PROVIDER =
endif

if JPEG2000
JPEG2000_PROVIDER = libidirectfbimageprovider_jpeg2000.la
else
JPEG2000_PROVIDER = 
endif

if MPEG2
MPEG2_PROVIDER = libidirectfbimageprovider_mpeg2.la
else
MPEG2_PROVIDER = 
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/src	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\" $(LIBJPEG_CFLAGS) $(LIBPNG_CFLAGS)

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_dfiff.la	\
	$(GIF_PROVIDER_LTLIB)			\
	$(JPEG_PROVIDER_LTLIB)			\
	$(PNG_PROVIDER_LTLIB)			\
	$(IMLIB2_PROVIDER)			\
	$(PNM_PROVIDER)				\
	$(SVG_PROVIDER)				\
	$(BMP_PROVIDER)				\
	$(JPEG2000_PROVIDER)			\
	$(MPEG2_PROVIDER)
	
if BUILD_STATIC
idirectfbimageprovider_DATA = $(idirectfbimageprovider_LTLIBRARIES:.la=.o)
endif


libidirectfbimageprovider_png_la_SOURCES = idirectfbimageprovider_png.c

libidirectfbimageprovider_png_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBPNG_LIBS)

libidirectfbimageprovider_png_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_dfiff_la_SOURCES = idirectfbimageprovider_dfiff.c

libidirectfbimageprovider_dfiff_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
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


libidirectfbimageprovider_imlib2_la_SOURCES = idirectfbimageprovider_imlib2.c

libidirectfbimageprovider_imlib2_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(IMLIB2_LIBS)

libidirectfbimageprovider_imlib2_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_pnm_la_SOURCES = idirectfbimageprovider_pnm.c

libidirectfbimageprovider_pnm_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbimageprovider_pnm_la_LDFLAGS = -avoid-version -module 


libidirectfbimageprovider_svg_la_SOURCES = idirectfbimageprovider_svg.c

libidirectfbimageprovider_svg_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(SVG_LIBS) \
	$(ZLIB_LIBS)

libidirectfbimageprovider_svg_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_bmp_la_SOURCES = idirectfbimageprovider_bmp.c

libidirectfbimageprovider_bmp_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbimageprovider_bmp_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_jpeg2000_la_SOURCES = idirectfbimageprovider_jpeg2000.c

libidirectfbimageprovider_jpeg2000_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(JASPER_LIBS)

libidirectfbimageprovider_jpeg2000_la_LDFLAGS = -avoid-version -module


libidirectfbimageprovider_mpeg2_la_SOURCES = idirectfbimageprovider_mpeg2.c

libidirectfbimageprovider_mpeg2_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	mpeg2/libmpeg2decode.la

libidirectfbimageprovider_mpeg2_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make
