## Makefile.am for DirectFB/src/core/gfxcards/tdfx

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/src

tdfx_LTLIBRARIES = libdirectfb_tdfx.la

tdfxdir = $(MODULEDIR)/gfxdrivers

libdirectfb_tdfx_la_SOURCES =	\
	tdfx.c	\
	tdfx.h

libdirectfb_tdfx_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
