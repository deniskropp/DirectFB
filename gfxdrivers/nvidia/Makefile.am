## Makefile.am for DirectFB/src/core/gfxcards/nvidia

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

nvidia_LTLIBRARIES = libdirectfb_nvidia.la

if BUILD_STATIC
nvidia_DATA = $(nvidia_LTLIBRARIES:.la=.o)
endif

nvidiadir = $(MODULEDIR)/gfxdrivers

libdirectfb_nvidia_la_SOURCES = \
     nvidia.c	       \
     nvidia_2d.c      \
     nvidia_3d.c      \
     nvidia_primary.c \
     nvidia_overlay.c \
     nvidia.h         \
     nvidia_mmio.h    \
     nvidia_2d.h      \
     nvidia_3d.h

libdirectfb_nvidia_la_LDFLAGS = \
     $(SYSFS_LIBS)       \
     -export-dynamic     \
     -avoid-version      \
     $(DFB_LDFLAGS) -lm


include $(top_srcdir)/rules/libobject.make

