## Makefile.am for DirectFB/src/core

SUBDIRS = fusion

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


internalincludedir = $(INTERNALINCLUDEDIR)/core

internalinclude_HEADERS = \
	coredefs.h		\
	coretypes.h		\
	core.h			\
	fbdev.h			\
	fonts.h			\
	gfxcard.h		\
	input.h			\
	layers.h		\
	state.h			\
	surfaces.h		\
	surfacemanager.h	\
	sig.h			\
	windows.h		\
	vt.h


noinst_LTLIBRARIES = libdirectfb_core.la

libdirectfb_core_la_LIBADD = \
	fusion/libdirectfb_core_fusion.la

libdirectfb_core_la_SOURCES = \
	core.c			\
	fbdev.c			\
	fonts.c			\
	gfxcard.c		\
	input.c			\
	layers.c		\
	state.c			\
	surfaces.c		\
	surfacemanager.c	\
	sig.c			\
	windows.c		\
	vt.c
