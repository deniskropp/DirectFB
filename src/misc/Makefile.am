## Makefile.am for DirectFB/src/misc

if FBDEV_CORE
FBDEBUG_SRC = fbdebug.c
FBDEBUG_H   = fbdebug.h
else
FBDEBUG_SRC =
FBDEBUG_H   =
endif

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
	conf.h			\
	cpu_accel.h		\
	$(FBDEBUG_H)		\
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
	$(FBDEBUG_SRC)		\
	gfx_util.c		\
	mem.c			\
	memcpy.c		\
	tree.c			\
	utf8.c			\
	util.c
