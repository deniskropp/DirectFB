## Makefile.am for DirectFB/src/core/gfxcards/r100

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems

r100_LTLIBRARIES = libdirectfb_r100.la

if BUILD_STATIC
r100_DATA = $(r100_LTLIBRARIES:.la=.o)
endif

r100dir = $(MODULEDIR)/gfxdrivers

libdirectfb_r100_la_SOURCES = \
	r100.c		\
	r100_state.c	\
	r100_overlay.c	\
	r100.h		\
	r100_regs.h	\
	r100_mmio.h	\
	r100_state.h

libdirectfb_r100_la_LDFLAGS = \
	-export-dynamic	\
	-avoid-version		\
	$(DFB_LDFLAGS) -lm


include $(top_srcdir)/rules/libobject.make

