## Makefile.am for DirectFB/src/input

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_input.la

libdirectfb_input_la_SOURCES = \
	idirectfbinputdevice.c	\
	idirectfbinputdevice.h	\
	idirectfbinputbuffer.c	\
	idirectfbinputbuffer.h

libdirectfb_input_la_LIBADD = $(THREAD_LIBS)
