## Makefile.am for DirectFB/lib/fusion/shmalloc

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

if ENABLE_MULTI
noinst_LTLIBRARIES = libfusion_shmalloc.la
endif

libfusion_shmalloc_la_SOURCES = \
	shcalloc.c			\
	shfree.c			\
	shmalloc.c			\
	shmalloc_internal.h		\
	shrealloc.c
