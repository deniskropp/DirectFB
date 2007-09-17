## Makefile.am for DirectFB/gfxdrivers/omap

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/systems \
	-I$(top_srcdir)/src

omap_LTLIBRARIES = libdirectfb_omap.la

if BUILD_STATIC
omap_DATA = $(omap_LTLIBRARIES:.la=.o)
endif

omapdir = $(MODULEDIR)/gfxdrivers

libdirectfb_omap_la_SOURCES = \
	omap.c \
	omap.h \
	omap_primary.c

libdirectfb_omap_la_LDFLAGS = \
	-export-dynamic \
	-avoid-version \
	$(DFB_LDFLAGS)

libdirectfb_omap_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

include $(top_srcdir)/rules/libobject.make
