## Makefile.am for DirectFB/src/gfx/generic

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS) $(DFB_CFLAGS_OMIT_FRAME_POINTER)


if BUILDMMX
MMXSOURCES = generic_mmx.c
else
MMXSOURCES =
endif


noinst_LTLIBRARIES = libdirectfb_generic.la

libdirectfb_generic_la_SOURCES = \
	generic.c		\
	generic.h		\
	$(MMXSOURCES)
