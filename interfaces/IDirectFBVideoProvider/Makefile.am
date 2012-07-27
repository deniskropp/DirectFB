## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider

EXTRA_DIST = \
	videodev.h \
	videodev2.h

if V4L_PROVIDER
V4L_PROVIDER_LTLIB = libidirectfbvideoprovider_v4l.la
else
V4L_PROVIDER_LTLIB =
endif

if GIF_PROVIDER
GIF_PROVIDER_LTLIB = libidirectfbvideoprovider_gif.la
else
GIF_PROVIDER_LTLIB =
endif

if MNG_PROVIDER
MNG_PROVIDER_LTLIB = libidirectfbvideoprovider_mng.la
else
MNG_PROVIDER_LTLIB =
endif

if GSTREAMER_PROVIDER
GSTREAMER_PROVIDER_LTLIB = libidirectfbvideoprovider_gstreamer.la
else
GSTREAMER_PROVIDER_LTLIB =
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	$(GSTREAMER_INCL)


idirectfbvideoprovider_LTLIBRARIES =	\
	$(GSTREAMER_PROVIDER_LTLIB)	\
	$(GIF_PROVIDER_LTLIB)		\
	$(MNG_PROVIDER_LTLIB)		\
	$(V4L_PROVIDER_LTLIB)

if BUILD_STATIC
idirectfbvideoprovider_DATA = $(idirectfbvideoprovider_LTLIBRARIES:.la=.o)
endif


libidirectfbvideoprovider_gstreamer_la_SOURCES = idirectfbvideoprovider_gstreamer.c

libidirectfbvideoprovider_gstreamer_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_gstreamer_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(GSTREAMER_LIBS)


libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_v4l_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


libidirectfbvideoprovider_gif_la_SOURCES = idirectfbvideoprovider_gif.c

libidirectfbvideoprovider_gif_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_gif_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


libidirectfbvideoprovider_mng_la_SOURCES = idirectfbvideoprovider_mng.c

libidirectfbvideoprovider_mng_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_mng_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBMNG)

include $(top_srcdir)/rules/libobject.make
