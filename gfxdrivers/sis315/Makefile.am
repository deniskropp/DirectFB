## Makefile.am for DirectFB/src/core/gfxcards/sis315

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

sis315_LTLIBRARIES = libdirectfb_sis315.la

sis315dir = $(MODULEDIR)/gfxdrivers

libdirectfb_sis315_la_SOURCES =	\
	sis315.c		\
	sis315_mmio.c		\
	sis315_mmio.h		\
	sis315_regs.h

libdirectfb_sis315_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
