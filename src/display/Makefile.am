## Makefile.am for DirectFB/src/display

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/display

internalinclude_HEADERS = \
	idirectfbpalette.h		\
	idirectfbsurface.h		\
	idirectfbsurface_layer.h 	\
	idirectfbsurface_window.h 	\
	idirectfbdisplaylayer.h


noinst_LTLIBRARIES = libdirectfb_display.la

libdirectfb_display_la_SOURCES =	\
	idirectfbpalette.c		\
	idirectfbsurface.c		\
	idirectfbsurface_layer.c 	\
	idirectfbsurface_window.c 	\
	idirectfbdisplaylayer.c
