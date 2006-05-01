## Makefile.am for DirectFB/gfxdrivers/mach64

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/systems	\
	-I$(top_srcdir)/src

mach64_LTLIBRARIES = libdirectfb_mach64.la

if BUILD_STATIC
mach64_DATA = $(mach64_LTLIBRARIES:.la=.o)
endif

mach64dir = $(MODULEDIR)/gfxdrivers

libdirectfb_mach64_la_SOURCES =	\
	mach64.c		\
	mach64.h		\
	mach64_state.c		\
	mach64_state.h		\
	mach64_overlay.c	\
	regs.h			\
	mmio.h

libdirectfb_mach64_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_mach64_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
