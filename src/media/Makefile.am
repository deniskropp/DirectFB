## Makefile.am for DirectFB/src/media

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DFLUXED_ARGS_BYTES=@FLUXED_ARGS_BYTES@

internalincludedir = $(INTERNALINCLUDEDIR)/media

internalinclude_HEADERS = \
	idirectfbdatabuffer.h		\
	idirectfbdatabuffer_client.h	\
	idirectfbfont.h			\
	idirectfbimageprovider.h	\
	idirectfbimageprovider_client.h	\
	idirectfbvideoprovider.h


noinst_LTLIBRARIES = libdirectfb_media.la


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SOURCESs = 
else
BUILT_SOURCES = DataBuffer.h DataBuffer.cpp ImageProvider.h ImageProvider.cpp
NON_PURE_VOODOO_SOURCESs = \
	idirectfbdatabuffer_client.c	\
	idirectfbfont.c			\
	idirectfbimageprovider.c	\
	idirectfbimageprovider_client.c	\
	idirectfbvideoprovider.c	\
	DataBuffer.cpp			\
	DataBuffer.h			\
	DataBuffer_includes.h		\
	DataBuffer_real.cpp		\
	ImageProvider.cpp		\
	ImageProvider.h			\
	ImageProvider_includes.h	\
	ImageProvider_real.cpp
endif

distclean-local::
	rm -f DataBuffer.cpp DataBuffer.h
	rm -f ImageProvider.cpp ImageProvider.h

libdirectfb_media_la_SOURCES = \
	$(NON_PURE_VOODOO_SOURCESs)	\
	idirectfbdatabuffer.c		\
	idirectfbdatabuffer_file.c	\
	idirectfbdatabuffer_memory.c	\
	idirectfbdatabuffer_streamed.c

libdirectfb_media_la_LIBADD = $(LIBM)


FLUX_ARGS = -i --no-direct --static-args-bytes=FLUXED_ARGS_BYTES

include $(top_srcdir)/rules/flux_comp.make
