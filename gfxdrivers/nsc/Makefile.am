## Makefile.am for DirectFB/gfxdrivers/nsc

SUBDIRS = include

INCLUDES = \
	-I/usr/src/linux/drivers/video/nsc \
	-I/usr/src/linux/drivers/video/nsc/gfx \
	-I/usr/src/linux/drivers/video/nsc/panel \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/systems \
	-I$(top_srcdir)/gfxdrivers/nsc/include

nsc_LTLIBRARIES = libdirectfb_nsc.la

nscdir = $(MODULEDIR)/gfxdrivers

libdirectfb_nsc_la_SOURCES =	\
	nsc.c	\
        nsc_galfns.c 

libdirectfb_nsc_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
