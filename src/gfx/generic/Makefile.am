## Makefile.am for DirectFB/src/gfx/generic

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS_OMIT_FRAME_POINTER)


internalincludedir = $(INTERNALINCLUDEDIR)/gfx/generic

internalinclude_HEADERS = \
     generic.h

noinst_LTLIBRARIES = libdirectfb_generic.la

libdirectfb_generic_la_SOURCES = \
	generic.c		\
	generic.h		\
	generic_mmx.h	\
     yuvtbl.h

yuvtbl_gen: yuvtbl-gen.c
	$(CC) -O2 yuvtbl-gen.c -o yuvtbl-gen

yuvtbl.h: yuvtbl_gen
	./yuvtbl-gen > $(srcdir)/yuvtbl.h

