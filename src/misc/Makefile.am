## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
	conf.h			\
	cpu_accel.h		\
	fbdebug.h		\
	gfx_util.h		\
	mem.h			\
	memcpy.h		\
	tree.h			\
	utf8.h			\
	util.h


noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.c			\
	colorhash.c		\
	colorhash.h		\
	cpu_accel.c		\
	fbdebug.c		\
	gfx_util.c		\
	mem.c			\
	memcpy.c		\
	tree.c			\
	utf8.c			\
	util.c
