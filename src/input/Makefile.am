## Makefile.am for DirectFB/src/input

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src


internalincludedir = $(INTERNALINCLUDEDIR)/input

internalinclude_HEADERS = \
	idirectfbinputdevice.h	\
	idirectfbinputbuffer.h


noinst_LTLIBRARIES = libdirectfb_input.la

libdirectfb_input_la_SOURCES = \
	idirectfbinputdevice.c	\
	idirectfbinputbuffer.c
