## Makefile.am for DirectFB/src/core

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = libdirectfb_core.la

libdirectfb_core_la_LIBADD = \
	$(THREAD_LIBS)

libdirectfb_core_la_LDFLAGS = -export-dynamic

CFLAGS = $(DFB_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DLIBDIR=\"@LIBDIR@\"


libdirectfb_core_la_SOURCES = \
	coredefs.h		\
	core.c			\
	core.h			\
	fbdev.c			\
	fbdev.h			\
	fonts.c			\
	fonts.h			\
	gfxcard.c		\
	gfxcard.h		\
	input.c			\
	input.h			\
	layers.c		\
	layers.h		\
	reactor.c		\
	reactor.h		\
	state.c			\
	state.h			\
	surfaces.c		\
	surfaces.h		\
	surfacemanager.c	\
	surfacemanager.h	\
	sig.c			\
	sig.h			\
	windows.c		\
	windows.h		\
	vt.c			\
	vt.h
