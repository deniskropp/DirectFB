## Makefile.am for DirectFB/lib/fusion/shm

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"

if ENABLE_MULTI
SHMSOURCES = heap.c pool.c shm.c
else
SHMSOURCES = fake.c
endif

EXTRA_DIST = fake.c

noinst_LTLIBRARIES = libfusion_shm.la

libfusion_shm_la_SOURCES = \
	$(SHMSOURCES)		\
	shm_internal.h

includedir = @INCLUDEDIR@/fusion/shm

include_HEADERS = \
	pool.h			\
	shm.h
