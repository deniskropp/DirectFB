## Makefile.am for DirectFB/gfxdrivers/cle266

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS)

cle266_LTLIBRARIES = libdirectfb_cle266.la

if BUILD_STATIC
cle266_DATA = $(cle266_LTLIBRARIES:.la=.o)
endif

cle266dir = $(MODULEDIR)/gfxdrivers

libdirectfb_cle266_la_SOURCES =     \
	unichrome.c     unichrome.h     \
	uc_accel.c      uc_accel.h      \
	uc_hw.h                         \
	uc_hwset.c      uc_hwmap.c      \
	uc_state.c      uc_state.h      \
	uc_fifo.c       uc_fifo.h       \
	uc_overlay.c    uc_overlay.h    \
	uc_ovl_hwmap.c  uc_ovl_hwset.c  \
	uc_primary.c			\
	mmio.h          vidregs.h       \
	regs2d.h        regs3d.h

libdirectfb_cle266_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
