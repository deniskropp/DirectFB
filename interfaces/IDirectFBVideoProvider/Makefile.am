## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider


if HAVE_LINUX
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


include $(top_srcdir)/rules/libobject.make
