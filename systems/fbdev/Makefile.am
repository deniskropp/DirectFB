## Makefile.am for DirectFB/systems/fbdev

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src


internalincludedir = $(INTERNALINCLUDEDIR)/fbdev

internalinclude_HEADERS = \
	fbdev.h		\
	vt.h


systemsdir = $(MODULEDIR)/systems

systems_LTLIBRARIES = libdirectfb_fbdev.la

libdirectfb_fbdev_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_fbdev_la_SOURCES = \
	fbdev.c		\
	vt.c
