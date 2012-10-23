## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider

DVC_CFLAGS = -I$(top_srcdir)/lib/dvc
DVC_LIBS = $(top_builddir)/lib/dvc/libdvc.la

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\" $(DVC_CFLAGS) \
	$(AVIFILE_CFLAGS) $(XINE_CFLAGS) $(XINE_VDPAU_CFLAGS) \
	$(SWFDEC_CFLAGS) $(FFMPEG_CFLAGS)

EXTRA_DIST = \
	videodev.h \
	videodev2.h \
	xine-lib-1.2-vdpau-hooks.patch

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

if ENABLE_FUSIONSOUND
FUSIONSOUND_LIBS = $(top_builddir)/lib/fusionsound/libfusionsound.la
else
FUSIONSOUND_LIBS =
endif

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

if OPENQUICKTIME_PROVIDER
OPENQUICKTIME_PROVIDER_LTLIB = libidirectfbvideoprovider_openquicktime.la
else
OPENQUICKTIME_PROVIDER_LTLIB =
endif

if XINE_PROVIDER
SUBDIRS = video_out_dfb
XINE_PROVIDER_LTLIB = libidirectfbvideoprovider_xine.la
else
XINE_PROVIDER_LTLIB =
endif

if XINE_VDPAU_PROVIDER
XINE_VDPAU_PROVIDER_LTLIB = libidirectfbvideoprovider_xine_vdpau.la
else
XINE_VDPAU_PROVIDER_LTLIB =
endif

if SWFDEC_PROVIDER
SWFDEC_PROVIDER_LTLIB = libidirectfbvideoprovider_swfdec.la
else
SWFDEC_PROVIDER_LTLIB =
endif

if FFMPEG_PROVIDER
FFMPEG_PROVIDER_LTLIB = libidirectfbvideoprovider_ffmpeg.la
else
FFMPEG_PROVIDER_LTLIB =
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusionsound	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	$(GSTREAMER_INCL)


idirectfbvideoprovider_LTLIBRARIES =	\
	$(GSTREAMER_PROVIDER_LTLIB)	\
	$(GIF_PROVIDER_LTLIB)		\
	$(MNG_PROVIDER_LTLIB)		\
	$(V4L_PROVIDER_LTLIB)		\
	$(AVIFILE_PROVIDER_LTLIB)	\
	$(LIBMPEG3_PROVIDER_LTLIB)	\
	$(FLASH_PROVIDER_LTLIB)		\
	$(OPENQUICKTIME_PROVIDER_LTLIB)	\
	$(XINE_PROVIDER_LTLIB)		\
	$(XINE_VDPAU_PROVIDER_LTLIB)	\
	$(SWFDEC_PROVIDER_LTLIB)	\
	$(FFMPEG_PROVIDER_LTLIB)

if BUILD_STATIC
idirectfbvideoprovider_DATA = $(idirectfbvideoprovider_LTLIBRARIES:.la=.o)
endif


libidirectfbvideoprovider_gstreamer_la_SOURCES = idirectfbvideoprovider_gstreamer.c

libidirectfbvideoprovider_gstreamer_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_gstreamer_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(GSTREAMER_LIBS) \
	$(FUSIONSOUND_LIBS)


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


libidirectfbvideoprovider_avifile_la_SOURCES = idirectfbvideoprovider_avifile.cpp

libidirectfbvideoprovider_avifile_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_avifile_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(AVIFILE_LIBS)


libidirectfbvideoprovider_libmpeg3_la_SOURCES = idirectfbvideoprovider_libmpeg3.c

libidirectfbvideoprovider_libmpeg3_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_libmpeg3_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(LIBMPEG3_LIBS) -lm


libidirectfbvideoprovider_swf_la_SOURCES = idirectfbvideoprovider_swf.c

libidirectfbvideoprovider_swf_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_swf_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(FLASH_LIBS)


libidirectfbvideoprovider_openquicktime_la_SOURCES = idirectfbvideoprovider_openquicktime.c

libidirectfbvideoprovider_openquicktime_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_openquicktime_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(OPENQUICKTIME_LIBS)


libidirectfbvideoprovider_xine_la_SOURCES = idirectfbvideoprovider_xine.c

libidirectfbvideoprovider_xine_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_xine_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(XINE_VDPAU_LIBS)


libidirectfbvideoprovider_xine_vdpau_la_SOURCES = idirectfbvideoprovider_xine_vdpau.c

libidirectfbvideoprovider_xine_vdpau_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_xine_vdpau_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(XINE_LIBS)


libidirectfbvideoprovider_swfdec_la_SOURCES = idirectfbvideoprovider_swfdec.c

libidirectfbvideoprovider_swfdec_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_swfdec_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(SWFDEC_LIBS) $(RT_LIBS)


libidirectfbvideoprovider_ffmpeg_la_SOURCES = idirectfbvideoprovider_ffmpeg.c

libidirectfbvideoprovider_ffmpeg_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_ffmpeg_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(FFMPEG_LIBS) $(RT_LIBS) $(DVC_LIBS)


include $(top_srcdir)/rules/libobject.make
