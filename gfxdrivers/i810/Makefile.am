## Makefile.am for DirectFB/src/core/gfxcards/i810

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

i810_LTLIBRARIES = libdirectfb_i810.la

if BUILD_STATIC
i810_DATA = $(i810_LTLIBRARIES:.la=.o)
endif

i810dir = $(MODULEDIR)/gfxdrivers

libdirectfb_i810_la_SOURCES =	\
	i810.c	\
	i810.h \
	i810_overlay.c

libdirectfb_i810_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_i810_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

