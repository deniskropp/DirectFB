## Makefile.am for DirectFB/src/media

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/media

internalinclude_HEADERS = \
	idirectfbdatabuffer.h		\
	idirectfbfont.h			\
	idirectfbimageprovider.h	\
	idirectfbvideoprovider.h


noinst_LTLIBRARIES = libdirectfb_media.la

libdirectfb_media_la_SOURCES = \
	idirectfbdatabuffer.c		\
	idirectfbdatabuffer_file.c	\
	idirectfbdatabuffer_memory.c	\
	idirectfbdatabuffer_streamed.c	\
	idirectfbfont.c			\
	idirectfbimageprovider.c
