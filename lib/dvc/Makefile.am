## Makefile.am for DirectFB-extra/lib/dvd

INCLUDES = $(DFB_CFLAGS)

AM_CFLAGS = -fomit-frame-pointer

noinst_LTLIBRARIES = libdvc.la

libdvc_la_SOURCES = dvc.c dvc.h dvc_mmx.h
libdvc_la_LIBADD  = $(DFB_LIBS)

noinst_PROGRAMS = dvc_test

dvc_test_SOURCES = dvc_test.c
dvc_test_LDADD   = libdvc.la

