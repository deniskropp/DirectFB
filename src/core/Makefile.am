## Makefile.am for DirectFB/src/core

SUBDIRS = fbdev fusion

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


internalincludedir = $(INTERNALINCLUDEDIR)/core

internalinclude_HEADERS = \
	coredefs.h		\
	coretypes.h		\
	core.h			\
	fonts.h			\
	gfxcard.h		\
	graphics_driver.h	\
	input.h			\
	input_driver.h		\
	layers.h		\
	palette.h		\
	state.h			\
	surfaces.h		\
	surfacemanager.h	\
	sig.h			\
	thread.h		\
	windows.h


noinst_LTLIBRARIES = libdirectfb_core.la

libdirectfb_core_la_LIBADD = \
	fbdev/libdirectfb_core_fbdev.la	\
	fusion/libdirectfb_core_fusion.la

libdirectfb_core_la_SOURCES = \
	core.c			\
	fonts.c			\
	gfxcard.c		\
	input.c			\
	layers.c		\
	palette.c		\
	state.c			\
	surfaces.c		\
	surfacemanager.c	\
	sig.c			\
	thread.c		\
	windows.c
