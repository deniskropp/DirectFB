## Makefile.am for DirectFB/src/core/gfxcards/radeon

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

radeon_LTLIBRARIES = libdirectfb_radeon.la

radeondir = $(MODULEDIR)/gfxdrivers

libdirectfb_radeon_la_SOURCES =	\
	radeon.c		\
	radeon.h		\
	radeon_state.c		\
	radeon_state.h		\
	radeon_regs.h		\
	radeon_mmio.h

libdirectfb_radeon_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
