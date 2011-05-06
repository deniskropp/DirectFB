# Makefile.am for DirectFB/systems/x11vdpau

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/gfxdrivers	\
	$(X11VDPAU_CFLAGS)


x11vdpau_includedir = $(includedir)/directfb-internal/x11vdpau
systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_x11vdpau.o
endif

systems_LTLIBRARIES = libdirectfb_x11vdpau.la

libdirectfb_x11vdpau_la_LDFLAGS = \
	$(X11VDPAU_LIBS)	\
	-avoid-version	\
	-module

x11vdpau_include_HEADERS = \
	primary.h		\
	x11.h			\
	x11vdpau_surface_pool.h	\
	x11types.h

libdirectfb_x11vdpau_la_SOURCES = \
	primary.c		\
	x11.c			\
	x11input.c		\
	x11vdpau_surface_pool.c

libdirectfb_x11vdpau_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

