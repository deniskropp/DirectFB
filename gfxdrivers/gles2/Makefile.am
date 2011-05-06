## Makefile.am for DirectFB/src/core/gfxcards/gles2

INCLUDES = \
	-I$(top_builddir)/include  \
	-I$(top_builddir)/lib	   \
	-I$(top_srcdir)/include	   \
	-I$(top_srcdir)/lib	   \
	-I$(top_srcdir)/src	   \
	-I$(top_srcdir)/systems    \
	-I$(top_srcdir)/gfxdrivers \
	$(GLES2_CFLAGS)

gles2_LTLIBRARIES = libdirectfb_gles2.la

if BUILD_STATIC
gles2_DATA = $(gles2_LTLIBRARIES:.la=.o)
endif

gles2dir = $(MODULEDIR)/gfxdrivers

libdirectfb_gles2_la_SOURCES =	\
	gles2_2d.c		\
	gles2_2d.h		\
	gles2_gfxdriver.c	\
	gles2_gfxdriver.h       \
	gles2_shaders.c         \
	gles2_shaders.h

libdirectfb_gles2_la_LDFLAGS = \
	-export-dynamic	       \
	-avoid-version	       \
	$(DFB_LDFLAGS) $(GLES2_LIBS)

libdirectfb_gles2_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la

include $(top_srcdir)/rules/libobject.make
