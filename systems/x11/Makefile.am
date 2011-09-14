# Makefile.am for DirectFB/systems/x11

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/gfxdrivers	\
	$(X11_CFLAGS)


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_x11.o
endif

systems_LTLIBRARIES = libdirectfb_x11.la

libdirectfb_x11_la_LDFLAGS = \
	$(X11_LIBS)	\
	-avoid-version	\
	-module

if GFX_GLX
libdirectfb_x11_la_LDFLAGS += -lGL
endif

libdirectfb_x11_la_SOURCES = \
	idirectfbgl.c		\
	primary.c		\
	primary.h		\
	surfacemanager.c	\
	surfacemanager.h	\
	vpsmem_surface_pool.c	\
	vpsmem_surface_pool.h	\
	x11.c			\
	x11.h			\
	x11image.c		\
	x11image.h		\
	x11input.c		\
	x11_surface_pool.c	\
	x11_surface_pool.h	\
	x11types.h		\
	xwindow.h		\
	xwindow.c

if GFX_GLX
libdirectfb_x11_la_SOURCES += \
	glx_surface_pool.c	\
	glx_surface_pool.h	\
	x11_surface_pool_bridge.c	\
	x11_surface_pool_bridge.h
endif

libdirectfb_x11_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

