## Makefile.am for FusionDale/src/misc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusiondale	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusiondale

AM_CPPFLAGS = \
	-DSYSCONFDIR=\"@sysconfdir@\"	\
	-DFUSIONDALE_USE_ONE="(@DIRECTFB_BUILD_ONE@)"

noinst_LTLIBRARIES = \
	libfusiondalemisc.la

libfusiondalemisc_la_SOURCES = \
	dale_config.c		\
	dale_config.h
