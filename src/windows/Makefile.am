## Makefile.am for DirectFB/src/windows

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_windows.la

libdirectfb_windows_la_SOURCES = \
	idirectfbwindow.c	\
	idirectfbwindow.h

libdirectfb_windows_la_LIBADD = $(THREAD_LIBS)
