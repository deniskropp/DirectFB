## Makefile.am for DirectFB/src/core/fbdev

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS) 

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""


internalincludedir = $(INTERNALINCLUDEDIR)/core/fbdev

internalinclude_HEADERS = \
	fbdev.h			\
	vt.h


noinst_LTLIBRARIES = libdirectfb_core_fbdev.la

libdirectfb_core_fbdev_la_SOURCES = \
	fbdev.c			\
	vt.c
