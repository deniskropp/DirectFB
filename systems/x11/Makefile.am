# Makefile.am for DirectFB/systems/x11

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(X11_CFLAGS)


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_x11.o
endif

systems_LTLIBRARIES = libdirectfb_x11.la


inputdriversdir = $(MODULEDIR)/inputdrivers

if BUILD_STATIC
inputdrivers_DATA = libdirectfb_x11input.o
endif

inputdrivers_LTLIBRARIES = libdirectfb_x11input.la


libdirectfb_x11_la_LDFLAGS = \
	$(X11_LIBS)	\
	-avoid-version	\
	-module

libdirectfb_x11_la_SOURCES = \
	primary.c	\
	primary.h	\
	x11.c		\
	x11.h		\
	xwindow.h	\
	xwindow.c

libdirectfb_x11_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la

libdirectfb_x11input_la_LDFLAGS = \
	$(X11_LIBS)	\
	-avoid-version	\
	-module

libdirectfb_x11input_la_SOURCES = \
	x11input.c

libdirectfb_x11input_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

