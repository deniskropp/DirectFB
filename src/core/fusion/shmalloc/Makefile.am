## Makefile.am for DirectFB/src/core/fusion

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


noinst_LTLIBRARIES = libdirectfb_shmalloc.la

libdirectfb_shmalloc_la_SOURCES = \
	mstats.c			\
	shcalloc.c			\
	shfree.c			\
	shmalloc.c			\
	shmalloc_internal.h		\
	shmemalign.c			\
	shrealloc.c			\
	shvalloc.c
