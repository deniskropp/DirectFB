## Makefile.am for DirectFB/src/media

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_media.la

libdirectfb_media_la_SOURCES = \
	idirectfbfont.c		\
	idirectfbfont.h

libdirectfb_media_la_LIBADD = $(THREAD_LIBS)
