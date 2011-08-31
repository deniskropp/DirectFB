## Makefile.am for DirectFB/lib/dvd

INCLUDES = \
        -I$(top_builddir)/include       \
        -I$(top_builddir)/lib           \
        -I$(top_srcdir)/include         \
        -I$(top_srcdir)/lib		\
        -I$(top_srcdir)/src

AM_CFLAGS = -fomit-frame-pointer

noinst_LTLIBRARIES = libdvc.la

libdvc_la_SOURCES = dvc.c dvc.h dvc_mmx.h

libdvc_la_LDFLAGS = $(DFB_LDFLAGS)

libdvc_la_LIBADD  = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la

noinst_PROGRAMS = dvc_test

dvc_test_SOURCES = dvc_test.c
dvc_test_LDADD   = libdvc.la
