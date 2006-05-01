## Makefile.am for DirectFB-internal/gfxdrivers/savage

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

savage_LTLIBRARIES = libdirectfb_savage.la

if BUILD_STATIC
savage_DATA = $(savage_LTLIBRARIES:.la=.o)
endif

savagedir = $(MODULEDIR)/gfxdrivers

libdirectfb_savage_la_SOURCES = \
	mmio.h			\
	savage.c		\
	savage.h		\
	savage3d.c		\
	savage3d.h		\
	savage4.c		\
	savage4.h		\
	savage2000.c		\
	savage2000.h		\
	savage_bci.h		\
	savage_streams_old.h

libdirectfb_savage_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_savage_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
