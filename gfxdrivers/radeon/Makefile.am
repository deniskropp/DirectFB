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
	radeon.c			\
	r100_state.c		\
	r200_state.c		\
	r300_state.c		\
	radeon_2d.c		\
	r100_3d.c			\
	r200_3d.c			\
	radeon_overlay.c	\
	radeon_crtc2.c		\
	radeon.h			\
	radeon_chipsets.h	\
	radeon_regs.h		\
	radeon_mmio.h		\
	radeon_state.h		\
	radeon_2d.h		\
	radeon_3d.h

libdirectfb_radeon_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS) -lm

libdirectfb_radeon_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
