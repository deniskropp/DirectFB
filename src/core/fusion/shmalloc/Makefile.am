## Makefile.am for DirectFB/src/core/fusion

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


noinst_LTLIBRARIES = libdirectfb_shmalloc.la

libdirectfb_shmalloc_la_SOURCES = \
	shcalloc.c			\
	shfree.c			\
	shmalloc.c			\
	shmalloc_internal.h		\
	shrealloc.c
