## Makefile.am for DirectFB/systems/osx

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

systemsdir = $(MODULEDIR)/systems

systems_LTLIBRARIES = libdirectfb_osx.la

libdirectfb_osx_la_LDFLAGS = \
	-framework CoreGraphics	\
	-avoid-version	\
	-module

libdirectfb_osx_la_SOURCES = \
	primary.c	\
	primary.h	\
	osx.c		\
	osx.h

inputdriversdir = $(MODULEDIR)/inputdrivers

inputdrivers_LTLIBRARIES = libdirectfb_osxinput.la

libdirectfb_osxinput_la_LDFLAGS = \
	-framework Carbon	\
	-avoid-version	\
	-module

libdirectfb_osxinput_la_SOURCES = \
	osxinput.c
