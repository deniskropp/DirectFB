## Makefile.am for DirectFB/src/core/gfxcards/ati128

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src


atidir = $(MODULEDIR)/gfxdrivers

ati_LTLIBRARIES = libdirectfb_ati128.la

if BUILD_STATIC
ati_DATA = $(ati_LTLIBRARIES:.la=.o)
endif


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


include $(top_srcdir)/rules/libobject.make

