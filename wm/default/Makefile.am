## Makefile.am for DirectFB/wm/default

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src


wmdir = $(MODULEDIR)/wm

wm_LTLIBRARIES = libdirectfbwm_default.la

libdirectfbwm_default_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfbwm_default_la_SOURCES = \
	default.c
