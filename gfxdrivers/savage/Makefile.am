## Makefile.am for DirectFB-internal/gfxdrivers/savage

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

savage_LTLIBRARIES = libdirectfb_savage.la

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
	savage_streams_old.c	\
	savage_streams_old.h

libdirectfb_savage_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)
