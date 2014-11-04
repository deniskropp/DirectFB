## Makefile.am for DirectFB/src/core/gfxcards/vsp1

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems		\
	$(VSP1_CFLAGS)


vsp1_LTLIBRARIES = libdirectfb_vsp1.la

if BUILD_STATIC
vsp1_DATA = $(vsp1_LTLIBRARIES:.la=.o)
endif

vsp1dir = $(MODULEDIR)/gfxdrivers


libdirectfb_vsp1_la_SOURCES =		\
	vsp1_blt.c			\
	vsp1_blt.h			\
	vsp1_driver.c			\
	vsp1_driver.h			\
	vsp1_types.h			\
	vsp-renderer.c			\
	media-ctl/libmediactl.c		\
	media-ctl/libv4l2subdev.c	\
	media-ctl/mediactl-priv.h	\
	media-ctl/mediactl.h		\
	media-ctl/tools.h		\
	media-ctl/v4l2subdev.h

libdirectfb_vsp1_la_LDFLAGS = \
	-module			\
	-avoid-version

libdirectfb_vsp1_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la	\
	$(VSP1_LIBS)


include $(top_srcdir)/rules/libobject.make

