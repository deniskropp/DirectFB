## Makefile.am for DirectFB/src/core/gfxcards/nvidia

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

nvidia_LTLIBRARIES = libdirectfb_nvidia.la

nvidiadir = $(MODULEDIR)/gfxdrivers

libdirectfb_nvidia_la_SOURCES =	\
	nvidia.c	 \
	nvidia_overlay.c \
	nvidia.h

libdirectfb_nvidia_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS) -lm
