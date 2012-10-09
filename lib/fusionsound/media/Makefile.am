## Makefile.am for lib/fusionsound/media

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusionsound \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib/	\
	-I$(top_srcdir)/lib/fusionsound

internalincludedir = $(GENERIC_INCLUDEDIR)/fusionsound-internal/media

internalinclude_HEADERS = \
	ifusionsoundmusicprovider.h

noinst_LTLIBRARIES = \
	libfusionsoundmedia.la

libfusionsoundmedia_la_SOURCES = \
	ifusionsoundmusicprovider.h \
	ifusionsoundmusicprovider.c
