## Makefile.am for DirectFB/gfxdrivers/mach64

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

mach64_LTLIBRARIES = libdirectfb_mach64.la

mach64dir = $(MODULEDIR)/gfxdrivers

libdirectfb_mach64_la_SOURCES =	\
	mach64.c		\
	mach64_state.c		\
	mach64.h		\
	regs.h			\
	mmio.h

libdirectfb_mach64_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
