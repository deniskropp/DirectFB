## Makefile.am for DirectFB/gfxdrivers/cyber5k

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

cyber5k_LTLIBRARIES = libdirectfb_cyber5k.la

cyber5kdir = $(MODULEDIR)/gfxdrivers

libdirectfb_cyber5k_la_SOURCES = \
	cyber5k.c	\
	cyber5k.h	\
	cyber5k_overlay.h \
	cyber5k_overlay.c \
	cyber5k_underlay.c \
	cyber5k_alpha.c \
	cyber5k_alpha.h \
	regs.h		\
	mmio.h

libdirectfb_cyber5k_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)
