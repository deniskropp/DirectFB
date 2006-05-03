## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider

EXTRA_DIST = videodev.h videodev2.h

if V4L_PROVIDER
V4L_PROVIDER_LTLIB = libidirectfbvideoprovider_v4l.la
else
V4L_PROVIDER_LTLIB =
endif

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"


idirectfbvideoprovider_LTLIBRARIES =	\
	$(V4L_PROVIDER_LTLIB)

if BUILD_STATIC
idirectfbvideoprovider_DATA = $(idirectfbvideoprovider_LTLIBRARIES:.la=.o)
endif

libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module

libidirectfbvideoprovider_v4l_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
