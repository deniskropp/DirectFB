## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
	conf.h			\
	gfx_util.h		\
	tree.h			\
	utf8.h			\
	util.h


noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.c			\
	gfx_util.c		\
	tree.c			\
	utf8.c			\
	util.c

libdirectfb_misc_la_LIBADD = $(THREAD_LIBS)
