## Makefile.am for DirectFB/gfxdrivers/neomagic

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

neomagic_LTLIBRARIES = libdirectfb_neomagic.la

if BUILD_STATIC
neomagic_DATA = $(neomagic_LTLIBRARIES:.la=.o)
endif

neomagicdir = $(MODULEDIR)/gfxdrivers

libdirectfb_neomagic_la_SOURCES =	\
	neomagic.c	\
	neomagic.h	\
	neo_overlay.c	\
	neo2200.c

libdirectfb_neomagic_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_neomagic_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
