## Makefile.am for DirectFB/src/core/gfxcards/sis315

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

ati_LTLIBRARIES = libdirectfb_sis315.la

atidir = $(MODULEDIR)/gfxdrivers

libdirectfb_sis315_la_SOURCES =	\
	sis315.c		\
	sis315_mmio.c

libdirectfb_sis315_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
