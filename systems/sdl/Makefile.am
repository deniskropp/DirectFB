## Makefile.am for DirectFB/systems/sdl

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(SDL_CFLAGS)


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_sdl.o
endif

systems_LTLIBRARIES = libdirectfb_sdl.la


inputdriversdir = $(MODULEDIR)/inputdrivers

if BUILD_STATIC
inputdrivers_DATA = libdirectfb_sdlinput.o
endif

inputdrivers_LTLIBRARIES = libdirectfb_sdlinput.la


libdirectfb_sdl_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_sdl_la_SOURCES = \
	primary.c	\
	primary.h	\
	sdl.c		\
	sdl.h

libdirectfb_sdl_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(SDL_LIBS)

libdirectfb_sdlinput_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_sdlinput_la_SOURCES = \
	sdlinput.c

libdirectfb_sdlinput_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la \
	$(SDL_LIBS)


include $(top_srcdir)/rules/libobject.make

