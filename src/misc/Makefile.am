## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DMODULEDIR=\"@MODULEDIR@\"	\
	-DSOPATH=\"@SOPATH@\"

internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
	conf.h			\
	gfx_util.h		\
	tree.h			\
	util.h


noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.c			\
	gfx_util.c		\
	tree.c			\
	util.c
