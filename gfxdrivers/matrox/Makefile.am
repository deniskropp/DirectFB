## Makefile.am for DirectFB/src/core/gfxcards/matrox

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

matrox_LTLIBRARIES = libdirectfb_matrox.la

matroxdir = $(MODULEDIR)/gfxdrivers

libdirectfb_matrox_la_SOURCES =	\
	matrox.c	\
	matrox.h	\
	matrox_bes.c	\
	matrox_state.c	\
	matrox_state.h	\
	regs.h		\
	mmio.h

libdirectfb_matrox_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
