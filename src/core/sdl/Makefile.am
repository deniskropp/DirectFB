## Makefile.am for DirectFB/src/core/sdl

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS) `sdl-config --cflags`

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""

noinst_LTLIBRARIES = libdirectfb_core_sdl.la

libdirectfb_core_sdl_la_LDFLAGS = `sdl-config --libs`

libdirectfb_core_sdl_la_SOURCES = \
	primary.c	\
	primary.h	\
	sdl.c		\
	sdlinput.c
