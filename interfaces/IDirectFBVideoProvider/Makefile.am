## Makefile.am for DirectFB/interfaces/IDirectFBVideoProvider

idirectfbvideoproviderdir = $(MODULEDIR)/interfaces/IDirectFBVideoProvider


if HAVE_LINUX
V4L_PROVIDER_LTLIB = libidirectfbvideoprovider_v4l.la
else
V4L_PROVIDER_LTLIB =
endif

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"


idirectfbvideoprovider_LTLIBRARIES =	\
	$(V4L_PROVIDER_LTLIB)


libidirectfbvideoprovider_v4l_la_SOURCES = idirectfbvideoprovider_v4l.c

libidirectfbvideoprovider_v4l_la_LDFLAGS = -avoid-version -module
