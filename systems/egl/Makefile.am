## Makefile.am for DirectFB/systems/egl

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(EGL_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/egl

internalinclude_HEADERS = \
	egl_primary.h		\
	egl_system.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_egl.o
endif
systems_LTLIBRARIES = libdirectfb_egl.la

libdirectfb_egl_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_egl_la_SOURCES = \
	egl_primary.c	\
	egl_system.c	\
	egl_surface_pool.c

libdirectfb_egl_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la	\
	$(EGL_LIBS)

include $(top_srcdir)/rules/libobject.make
