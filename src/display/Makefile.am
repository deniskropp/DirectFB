## Makefile.am for DirectFB/src/display

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_display.la

libdirectfb_display_la_SOURCES =	\
	idirectfbsurface.c		\
	idirectfbsurface.h		\
	idirectfbsurface_layer.c 	\
	idirectfbsurface_layer.h 	\
	idirectfbsurface_window.c 	\
	idirectfbsurface_window.h 	\
	idirectfbdisplaylayer.c		\
	idirectfbdisplaylayer.h

libdirectfb_display_la_LIBADD = $(THREAD_LIBS)
