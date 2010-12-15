## Makefile.am for DirectFB/src/core/gfxcards/gl

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

gl_LTLIBRARIES = libdirectfb_gl.la

if BUILD_STATIC
gl_DATA = $(gl_LTLIBRARIES:.la=.o)
endif

gldir = $(MODULEDIR)/gfxdrivers

libdirectfb_gl_la_SOURCES =	\
	gl_2d.c		\
	gl_2d.h		\
	gl_gfxdriver.c	\
	gl_gfxdriver.h

libdirectfb_gl_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS) -lGL -lX11

libdirectfb_gl_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
