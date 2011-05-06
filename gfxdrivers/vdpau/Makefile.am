## Makefile.am for DirectFB/src/core/gfxcards/vdpau

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

vdpau_LTLIBRARIES = libdirectfb_vdpau.la

if BUILD_STATIC
vdpau_DATA = $(vdpau_LTLIBRARIES:.la=.o)
endif

vdpaudir = $(MODULEDIR)/gfxdrivers

libdirectfb_vdpau_la_SOURCES =	\
	vdpau_2d.c		\
	vdpau_2d.h		\
	vdpau_gfxdriver.c	\
	vdpau_gfxdriver.h

libdirectfb_vdpau_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS) -lm

libdirectfb_vdpau_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

