## Makefile.am for DirectFB/src/core/gfxcards/gp2d

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems		\
	$(GP2D_CFLAGS)


gp2d_LTLIBRARIES = libdirectfb_gp2d.la

if BUILD_STATIC
gp2d_DATA = $(gp2d_LTLIBRARIES:.la=.o)
endif

gp2ddir = $(MODULEDIR)/gfxdrivers


libdirectfb_gp2d_la_SOURCES =	\
	gp2d_blt.c		\
	gp2d_blt.h		\
	gp2d_driver.c		\
	gp2d_driver.h		\
	gp2d_engine.cpp		\
	gp2d_engine.h		\
	gp2d_regs.h		\
	gp2d_types.h

libdirectfb_gp2d_la_LDFLAGS = \
	-module			\
	-avoid-version

libdirectfb_gp2d_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la	\
	$(GP2D_LIBS)


include $(top_srcdir)/rules/libobject.make

