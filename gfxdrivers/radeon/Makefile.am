## Makefile.am for DirectFB/src/core/gfxcards/radeon

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

radeon_LTLIBRARIES = libdirectfb_radeon.la

if BUILD_STATIC
radeon_DATA = $(radeon_LTLIBRARIES:.la=.o)
endif

radeondir = $(MODULEDIR)/gfxdrivers

libdirectfb_radeon_la_SOURCES =	\
	radeon.c		\
	radeon.h		\
	radeon_overlay.c	\
	radeon_state.c		\
	radeon_state.h		\
	radeon_regs.h		\
	radeon_mmio.h

libdirectfb_radeon_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
