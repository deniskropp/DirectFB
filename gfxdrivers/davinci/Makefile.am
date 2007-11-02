## Makefile.am for DirectFB/src/core/gfxcards/davinci

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

davinci_LTLIBRARIES = libdirectfb_davinci.la

if BUILD_STATIC
davinci_DATA = $(davinci_LTLIBRARIES:.la=.o)
endif

davincidir = $(MODULEDIR)/gfxdrivers

libdirectfb_davinci_la_SOURCES =	\
	davinci_2d.c		\
	davinci_2d.h		\
	davinci_gfxdriver.c	\
	davinci_gfxdriver.h	\
	davinci_osd.c		\
	davinci_osd.h		\
	davinci_screen.c	\
	davinci_screen.h

libdirectfb_davinci_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_davinci_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
