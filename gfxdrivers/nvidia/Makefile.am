## Makefile.am for DirectFB/src/core/gfxcards/nvidia

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

nvidia_LTLIBRARIES = libdirectfb_nvidia.la

nvidiadir = $(MODULEDIR)/gfxdrivers

libdirectfb_nvidia_la_SOURCES =	\
	nvidia.c	\
	nvidia.h

libdirectfb_nvidia_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
