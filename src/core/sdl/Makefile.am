## Makefile.am for DirectFB/src/core/sdl

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(SDL_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""


systemsdir = $(MODULEDIR)/systems
inputdriversdir = $(MODULEDIR)/inputdrivers

systems_LTLIBRARIES = libdirectfb_sdl.la
inputdrivers_LTLIBRARIES = libdirectfb_sdlinput.la

libdirectfb_sdl_la_LDFLAGS = \
	$(SDL_LIBS)	\
	-export-dynamic	\
	-avoid-version	\
	$(DFB_LDFLAGS)

libdirectfb_sdl_la_SOURCES = \
	primary.c	\
	primary.h	\
	sdl.c		\
	sdl.h

libdirectfb_sdlinput_la_LDFLAGS = \
	$(SDL_LIBS)	\
	-export-dynamic	\
	-avoid-version	\
	$(DFB_LDFLAGS)

libdirectfb_sdlinput_la_SOURCES = \
	sdlinput.c
