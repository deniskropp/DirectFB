## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.h			\
	conf.c			\
	util.h			\
	util.c			\
	gfx_util.h		\
	gfx_util.c

libdirectfb_misc_la_LIBADD = $(THREAD_LIBS)
