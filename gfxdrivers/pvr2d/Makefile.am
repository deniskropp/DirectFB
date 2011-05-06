## Makefile.am for DirectFB/src/core/gfxcards/pvr2d

INCLUDES = \
	-I$(top_builddir)/include  \
	-I$(top_builddir)/lib	   \
	-I$(top_srcdir)/include	   \
	-I$(top_srcdir)/lib	   \
	-I$(top_srcdir)/src	   \
	-I$(top_srcdir)/systems    \
	-I$(top_srcdir)/gfxdrivers \
	$(PVR2D_CFLAGS)

pvr2d_LTLIBRARIES = libdirectfb_pvr2d.la

if BUILD_STATIC
pvr2d_DATA = $(pvr2d_LTLIBRARIES:.la=.o)
endif

pvr2ddir = $(MODULEDIR)/gfxdrivers

libdirectfb_pvr2d_la_SOURCES =	\
	pvr2d_2d.c		\
	pvr2d_2d.h		\
	pvr2d_gfxdriver.c	\
	pvr2d_gfxdriver.h

libdirectfb_pvr2d_la_LDFLAGS = \
	-export-dynamic	       \
	-avoid-version	       \
	$(DFB_LDFLAGS) $(PVR2D_LIBS)

libdirectfb_pvr2d_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la

include $(top_srcdir)/rules/libobject.make
