## Makefile.am for DirectFB/src/core/fbdev

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""


internalincludedir = $(INTERNALINCLUDEDIR)/core/fbdev

internalinclude_HEADERS = \
	fbdev.h			\
	vt.h


systemsdir = $(MODULEDIR)/systems

systems_LTLIBRARIES = libdirectfb_fbdev.la

libdirectfb_fbdev_la_LDFLAGS = \
	-export-dynamic	\
	-avoid-version	\
	$(DFB_LDFLAGS)

libdirectfb_fbdev_la_SOURCES = \
	fbdev.c		\
	vt.c
