## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.h			\
	conf.c			\
	gfx_util.h		\
	gfx_util.c		\
	tree.h			\
	tree.c			\
	utf8.c			\
	utf8.h			\
	util.h			\
	util.c

libdirectfb_misc_la_LIBADD = $(THREAD_LIBS)
