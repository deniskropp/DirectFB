## Makefile.am for DirectFB/systems/osx

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

systemsdir = $(MODULEDIR)/systems

systems_LTLIBRARIES = libdirectfb_osx.la

if BUILD_STATIC
systems_DATA = libdirectfb_osx.o
endif


libdirectfb_osx_la_LDFLAGS = \
	-framework CoreGraphics	\
	-avoid-version	\
	-module

libdirectfb_osx_la_SOURCES = \
	primary.c	\
	primary.h	\
	osx.c		\
	osx.h

libdirectfb_osx_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la

inputdriversdir = $(MODULEDIR)/inputdrivers

inputdrivers_LTLIBRARIES = libdirectfb_osxinput.la

if BUILD_STATIC
inputdrivers_DATA = libdirectfb_osxinput.o
endif


libdirectfb_osxinput_la_LDFLAGS = \
	-framework Carbon	\
	-avoid-version	\
	-module

libdirectfb_osxinput_la_SOURCES = \
	osxinput.c

libdirectfb_osxinput_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make	

