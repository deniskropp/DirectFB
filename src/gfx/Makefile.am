## Makefile.am for DirectFB/src/misc

SUBDIRS = generic

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_gfx.la

libdirectfb_gfx_la_SOURCES =	\
	convert.h		\
	convert.c		\
	util.h			\
	util.c

libdirectfb_gfx_la_LIBADD = \
	generic/libdirectfb_generic.la	\
	$(THREAD_LIBS)
