## Makefile.am for DirectFB/src/media

INCLUDES = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src


internalincludedir = $(INTERNALINCLUDEDIR)/media

internalinclude_HEADERS = \
	idirectfbdatabuffer.h		\
	idirectfbfont.h			\
	idirectfbimageprovider.h	\
	idirectfbvideoprovider.h


noinst_LTLIBRARIES = libdirectfb_media.la


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SOURCESs = 
else
NON_PURE_VOODOO_SOURCESs = \
	idirectfbfont.c			\
	idirectfbimageprovider.c	\
	idirectfbvideoprovider.c
endif

libdirectfb_media_la_SOURCES = \
	$(NON_PURE_VOODOO_SOURCESs)	\
	idirectfbdatabuffer.c		\
	idirectfbdatabuffer_file.c	\
	idirectfbdatabuffer_memory.c	\
	idirectfbdatabuffer_streamed.c

