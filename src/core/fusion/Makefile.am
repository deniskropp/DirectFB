## Makefile.am for DirectFB/src/core/fusion

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


internalincludedir = $(INTERNALINCLUDEDIR)/core/fusion

internalinclude_HEADERS = \
	arena.h		\
	fusion.h	\
	list.h		\
	lock.h		\
	reactor.h	\
	ref.h		\
	shmalloc.h	\
	fusion_types.h


noinst_LTLIBRARIES = libdirectfb_core_fusion.la

libdirectfb_core_fusion_la_SOURCES = \
	arena.c			\
	fusion.c		\
	fusion_internal.h	\
	list.c			\
	lock.c			\
	reactor.c		\
	ref.c			\
	shmalloc.c		\
	util.c
