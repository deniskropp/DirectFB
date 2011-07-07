## Makefile.am for DirectFB/systems/mesa

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(MESA_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/mesa_system

internalinclude_HEADERS = \
	mesa_system.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_mesa_system.o
endif
systems_LTLIBRARIES = libdirectfb_mesa_system.la

libdirectfb_mesa_system_la_LDFLAGS = \
	-avoid-version	\
	-module \
	$(MESA_LIBS)

libdirectfb_mesa_system_la_SOURCES = \
	idirectfbgl2.c		\
	idirectfbgl2context.c	\
	mesa_layer.c		\
	mesa_screen.c		\
	mesa_system.c		\
	mesa_surface_pool.c     \
	vt.c			\
	vt.h

libdirectfb_mesa_system_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la

include $(top_srcdir)/rules/libobject.make
