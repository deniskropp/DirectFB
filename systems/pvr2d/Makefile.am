## Makefile.am for DirectFB/systems/pvr2d

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(PVR2D_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/pvr2d

internalinclude_HEADERS = \
	pvr2d_primary.h		\
	pvr2d_system.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_pvr2d.o
endif
systems_LTLIBRARIES = libdirectfb_pvr2d.la

libdirectfb_pvr2d_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_pvr2d_la_SOURCES = \
	pvr2d_primary.c	\
	pvr2d_system.c	\
	pvr2d_surface_pool.c

libdirectfb_pvr2d_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la	\
	$(PVR2D_LIBS)

include $(top_srcdir)/rules/libobject.make
