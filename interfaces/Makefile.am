## Makefile.am for DirectFB/interfaces

idirectfbfontdir          = $(LIBDIR)/interfaces/IDirectFBFont
idirectfbimageproviderdir = $(LIBDIR)/interfaces/IDirectFBImageProvider
idirectfbvideoproviderdir = $(LIBDIR)/interfaces/IDirectFBVideoProvider


if AVIFILE_PROVIDER
AVIFILE_PROVIDER_LTLIB = libidirectfbvideoprovider_avifile.la
else
AVIFILE_PROVIDER_LTLIB =
endif



## This is a hack around fucking libtool to link libmpeg3.a into our shared object.

if LIBMPEG3_PROVIDER

clean-local:
	-rm -f libidirectfbvideoprovider_libmpeg3.so

all-local: libidirectfbvideoprovider_libmpeg3.so

libidirectfbvideoprovider_libmpeg3.so: idirectfbvideoprovider_libmpeg3.o $(LIBMPEG3_DIR)/libmpeg3.a
	$(LD) -shared -export-dynamic -o $@ $+ $(THREAD_LIBS) -lm

idirectfbvideoprovider_SCRIPTS = libidirectfbvideoprovider_libmpeg3.so

LIBMPEG3_INCLUDES = -I$(LIBMPEG3_DIR)

else

clean-local:
all-local:

endif

EXTRA_DIST = idirectfbvideoprovider_libmpeg3.c


if FLASH_PROVIDER
FLASH_PROVIDER_LTLIB = libidirectfbvideoprovider_swf.la
else
FLASH_PROVIDER_LTLIB =
endif


INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src \
	-DDATADIR=\"@DATADIR@\"	\
	$(LIBMPEG3_INCLUDES)

CFLAGS = $(DFB_CFLAGS) $(FREETYPE_CFLAGS)
CXXFLAGS = $(DFB_CXXFLAGS) $(AVIFILE_CFLAGS)


idirectfbfont_LTLIBRARIES = \
	libidirectfbfont_default.la		\
	libidirectfbfont_ft2.la

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_png.la	\
	libidirectfbimageprovider_gif.la	\
	libidirectfbimageprovider_jpeg.la

idirectfbvideoprovider_LTLIBRARIES = \
	libidirectfbvideoprovider_v4l.la	\
	$(AVIFILE_PROVIDER_LTLIB)		\
	$(FLASH_PROVIDER_LTLIB)



libidirectfbfont_default_la_SOURCES = idirectfbfont_default.c

libidirectfbfont_default_la_LDFLAGS = -avoid-version -module -disable-static


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

libidirectfbvideoprovider_avifile_la_LIBADD = $(AVIFILE_LIBS)

libidirectfbvideoprovider_avifile_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbvideoprovider_swf_la_SOURCES = idirectfbvideoprovider_swf.c

libidirectfbvideoprovider_swf_la_LIBADD = $(LIBFLASH)

libidirectfbvideoprovider_swf_la_LDFLAGS = -avoid-version -module -disable-static


libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module -disable-static
