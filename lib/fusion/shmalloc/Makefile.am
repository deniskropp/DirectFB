## Makefile.am for DirectFB/lib/fusion/shmalloc

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


noinst_LTLIBRARIES = libfusion_shmalloc.la

libfusion_shmalloc_la_SOURCES = \
	shcalloc.c			\
	shfree.c			\
	shmalloc.c			\
	shmalloc_internal.h		\
	shrealloc.c
