## Makefile.am for DirectFB/gfxdrivers/unichrome

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/systems

AM_CFLAGS = $(DFB_CFLAGS)

unichrome_LTLIBRARIES = libdirectfb_unichrome.la

if BUILD_STATIC
unichrome_DATA = $(unichrome_LTLIBRARIES:.la=.o)
endif

unichromedir = $(MODULEDIR)/gfxdrivers

libdirectfb_unichrome_la_SOURCES =  \
	unichrome.c     unichrome.h     \
	uc_probe.h                      \
	uc_accel.c      uc_accel.h      \
	uc_hw.h                         \
	uc_hwset.c      uc_hwmap.c      \
	uc_state.c      uc_state.h      \
	uc_fifo.c       uc_fifo.h       \
	uc_overlay.c    uc_overlay.h    \
	uc_ovl_hwmap.c  uc_ovl_hwset.c  \
	uc_primary.c                    \
	uc_spic.c                       \
	uc_ioctl.h                      \
	mmio.h          vidregs.h       \
	regs2d.h        regs3d.h

libdirectfb_unichrome_la_LDFLAGS = \
	-export-dynamic		   \
	-avoid-version		   \
	$(DFB_LDFLAGS)

libdirectfb_unichrome_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la	\
	-lm


include $(top_srcdir)/rules/libobject.make
	
