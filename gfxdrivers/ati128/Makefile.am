## Makefile.am for DirectFB/src/core/gfxcards/ati128

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

ati_LTLIBRARIES = libdirectfb_ati128.la

atidir = $(MODULEDIR)/gfxdrivers

libdirectfb_ati128_la_SOURCES =	\
	ati128.c		\
	ati128.h		\
	ati128_overlay.c	\
	ati128_state.c		\
	ati128_state.h		\
	regs.h			\
	mmio.h

libdirectfb_ati128_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
