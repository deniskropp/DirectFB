## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DMODULEDIR=\"@MODULEDIR@\"	\
	-DSOPATH=\"@SOPATH@\"

internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
	conf.h			\
	gfx_util.h		\
	util.h


noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.c			\
	gfx_util.c		\
	util.c
