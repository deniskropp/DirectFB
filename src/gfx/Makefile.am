## Makefile.am for DirectFB/src/misc

SUBDIRS = generic

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/gfx

internalinclude_HEADERS = \
	convert.h		\
	util.h


noinst_LTLIBRARIES = libdirectfb_gfx.la

libdirectfb_gfx_la_SOURCES = \
	convert.c		\
	util.c

libdirectfb_gfx_la_LIBADD = \
	generic/libdirectfb_generic.la	\
	$(THREAD_LIBS)
