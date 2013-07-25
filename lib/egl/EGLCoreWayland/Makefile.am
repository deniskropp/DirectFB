# Makefile.am for DirectFB/lib/egl/EGLCoreWayland

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/egl	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/egl		\
	-I$(top_srcdir)/lib/wayland-dfb	\
	-I$(top_srcdir)/lib/wayland-egl	\
	-I$(top_srcdir)/src		\
	$(WAYLAND_CFLAGS)



dfbegl_coredir = $(MODULEDIR)/dfbegl_core

if BUILD_STATIC
dfbegl_core_DATA = libdfbegl_core_wayland.o
endif

dfbegl_core_LTLIBRARIES = libdfbegl_core_wayland.la


libdfbegl_core_wayland_la_LDFLAGS = \
	$(WAYLAND_LIBS)	\
	-avoid-version	\
	-module

libdfbegl_core_wayland_la_SOURCES = \
	EGLDisplayWayland.cpp		\
	EGLDisplayWayland.h

libdfbegl_core_wayland_la_LIBADD = \
	$(DFB_BASE_LIBS)	\
	$(libegl)		\
	$(libwayland_dfb)


include $(top_srcdir)/rules/libobject.make
include $(top_srcdir)/rules/libs_deps.make
