# Makefile.am for DirectFB/systems/x11

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/gfxdrivers	\
	$(X11_CFLAGS)			\
	-I$(top_srcdir)/lib/egl


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_x11.o
endif

systems_LTLIBRARIES = libdirectfb_x11.la


dfbegl_coredir = $(MODULEDIR)/dfbegl_core

if BUILD_STATIC
dfbegl_core_DATA = libdfbegl_core_x11.o
endif

dfbegl_core_LTLIBRARIES = libdfbegl_core_x11.la


libdirectfb_x11_la_LDFLAGS = \
	$(X11_LIBS) -lXcomposite	\
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
	x11_window_pool.c	\
	x11_window_pool.h	\
	x11types.h		\
	xwindow.h		\
	xwindow.c		\
	X11EGLImpl.cpp		\
	X11EGLImpl.h

if GFX_GLX
libdirectfb_x11_la_SOURCES += \
	glx_surface_pool.c	\
	glx_surface_pool.h	\
	x11_surface_pool_bridge.c	\
	x11_surface_pool_bridge.h
endif

libdirectfb_x11_la_LIBADD = \
	$(DFB_BASE_LIBS)	\
	$(libegl)		\
	$(libdfbegl)



libdfbegl_core_x11_la_LDFLAGS = \
	$(X11_LIBS)	\
	-avoid-version	\
	-module

libdfbegl_core_x11_la_SOURCES = \
	EGLDisplayX11.cpp		\
	EGLDisplayX11.h

libdfbegl_core_x11_la_LIBADD = \
	$(DFB_BASE_LIBS)	\
	$(libegl) -lxcb


include $(top_srcdir)/rules/libobject.make
include $(top_srcdir)/rules/libs_deps.make
