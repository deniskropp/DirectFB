## Makefile.am for DirectFB/lib/direct/os/linux/glibc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"	\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"


includedir = @INCLUDEDIR@/direct/os/linux/glibc

include_HEADERS = \
	filesystem.h		\
	mutex.h			\
	thread.h		\
	types.h			\
	waitqueue.h


noinst_LTLIBRARIES = libdirect_os_linux_glibc.la

libdirect_os_linux_glibc_la_SOURCES = \
	clock.c		\
	deprecated.c	\
	filesystem.c	\
	log.c		\
	mem.c		\
	mutex.c		\
	signals.c	\
	system.c	\
	thread.c	\
	util.c
