# Makefile.am for DirectFB/systems/x11vdpau

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/gfxdrivers	\
	$(X11VDPAU_CFLAGS)


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_x11vdpau.o
endif

systems_LTLIBRARIES = libdirectfb_x11vdpau.la

libdirectfb_x11vdpau_la_LDFLAGS = \
	$(X11VDPAU_LIBS)	\
	-avoid-version	\
	-module

libdirectfb_x11vdpau_la_SOURCES = \
	primary.c		\
	primary.h		\
	x11.c			\
	x11.h			\
	x11image.c		\
	x11image.h		\
	x11input.c		\
	x11_surface_pool.c	\
	x11_surface_pool.h	\
	x11vdpau_surface_pool.c	\
	x11vdpau_surface_pool.h	\
	x11types.h		\
	xwindow.h		\
	xwindow.c

libdirectfb_x11vdpau_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

