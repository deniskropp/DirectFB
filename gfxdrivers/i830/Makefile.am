## Makefile.am for DirectFB/src/core/gfxcards/i830

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

i830_LTLIBRARIES = libdirectfb_i830.la

if BUILD_STATIC
i830_DATA = $(i830_LTLIBRARIES:.la=.o)
endif

i830dir = $(MODULEDIR)/gfxdrivers

libdirectfb_i830_la_SOURCES =	\
	i830.c	\
	i830.h \
	i830_overlay.c

libdirectfb_i830_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_i830_la_LIBADD = -lm

include $(top_srcdir)/rules/libobject.make

