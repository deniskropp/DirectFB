## Makefile.am for DirectFB/src/core/fusion

if ENABLE_MULTI
SUBDIRS = shmalloc
endif

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
	object.h	\
	property.h	\
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
	object.c		\
	property.c		\
	reactor.c		\
	ref.c			\
	fusion_shmalloc.c	\
	util.c

if ENABLE_MULTI
libdirectfb_core_fusion_la_LIBADD = shmalloc/libdirectfb_shmalloc.la
endif
