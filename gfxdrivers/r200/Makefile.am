## Makefile.am for DirectFB/src/core/gfxcards/r200

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems

r200_LTLIBRARIES = libdirectfb_r200.la

if BUILD_STATIC
r200_DATA = $(r200_LTLIBRARIES:.la=.o)
endif

r200dir = $(MODULEDIR)/gfxdrivers

libdirectfb_r200_la_SOURCES = \
	r200.c		\
	r200_state.c	\
	r200_crtc1.c	\
	r200_overlay.c	\
	r200.h		\
	r200_regs.h	\
	r200_mmio.h	\
	r200_state.h

libdirectfb_r200_la_LDFLAGS = \
	-export-dynamic	\
	-avoid-version		\
	$(DFB_LDFLAGS) -lm


include $(top_srcdir)/rules/libobject.make

