## Makefile.am for FusionDale/src/core

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusiondale	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusiondale

AM_CPPFLAGS = \
     -DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"

noinst_LTLIBRARIES = \
	libfusiondalecore.la

coreincludedir = @GENERIC_INCLUDEDIR@/fusiondale/core

coreinclude_HEADERS = \
	dale_core.h	\
	dale_types.h	\
	messenger.h	\
	messenger_port.h

libfusiondalecore_la_SOURCES = \
	dale_core.c	\
	messenger.c	\
	messenger_port.c
