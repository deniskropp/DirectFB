## Makefile.am for DirectFB/src/core/gfxcards/i810

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS)

i810_LTLIBRARIES = libdirectfb_i810.la

i810dir = $(MODULEDIR)/gfxdrivers

libdirectfb_i810_la_SOURCES =	\
	i810.c	\
	i810.h \
	i810_overlay.c

libdirectfb_i810_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
