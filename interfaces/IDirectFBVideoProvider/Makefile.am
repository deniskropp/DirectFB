## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider


if AVIFILE_PROVIDER
AVIFILE_PROVIDER_LTLIB = libidirectfbvideoprovider_avifile.la
else
AVIFILE_PROVIDER_LTLIB =
endif


if LIBMPEG3_PROVIDER
LIBMPEG3_PROVIDER_LTLIB = libidirectfbvideoprovider_libmpeg3.la
else
LIBMPEG3_PROVIDER_LTLIB =
endif


if FLASH_PROVIDER
FLASH_PROVIDER_LTLIB = libidirectfbvideoprovider_swf.la
else
FLASH_PROVIDER_LTLIB =
endif


INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"

CFLAGS = $(DFB_CFLAGS)
CXXFLAGS = $(DFB_CXXFLAGS) $(AVIFILE_CFLAGS)


idirectfbvideoprovider_LTLIBRARIES = \
	libidirectfbvideoprovider_v4l.la	\
	$(AVIFILE_PROVIDER_LTLIB)		\
	$(FLASH_PROVIDER_LTLIB)			\
	$(LIBMPEG3_PROVIDER_LTLIB)


libidirectfbvideoprovider_avifile_la_SOURCES = idirectfbvideoprovider_avifile.cpp

libidirectfbvideoprovider_avifile_la_LIBADD = $(AVIFILE_LIBS)

libidirectfbvideoprovider_avifile_la_LDFLAGS = -avoid-version -module


libidirectfbvideoprovider_swf_la_SOURCES = idirectfbvideoprovider_swf.c

libidirectfbvideoprovider_swf_la_LIBADD = $(LIBFLASH)

libidirectfbvideoprovider_swf_la_LDFLAGS = -avoid-version -module


libidirectfbvideoprovider_libmpeg3_la_SOURCES = idirectfbvideoprovider_libmpeg3.c

libidirectfbvideoprovider_libmpeg3_la_LIBADD = $(LIBMPEG3)

libidirectfbvideoprovider_libmpeg3_la_LDFLAGS = -avoid-version -module


libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module
