## Makefile.am for lib/fusionsound/misc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusionsound

AM_CPPFLAGS = \
	-DSYSCONFDIR=\"@SYSCONFDIR@\"

internalincludedir = $(GENERIC_INCLUDEDIR)/fusionsound-internal/misc

internalinclude_HEADERS = \
	sound_conf.h \
	sound_util.h

noinst_LTLIBRARIES = \
	libfusionsoundmisc.la

libfusionsoundmisc_la_SOURCES = \
	sound_conf.c	\
	sound_conf.h	\
	sound_util.c	\
	sound_util.h
