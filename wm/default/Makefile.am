## Makefile.am for DirectFB/wm/default

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src


wmdir = $(MODULEDIR)/wm

if BUILD_STATIC
wm_DATA = libdirectfbwm_default.o
endif

wm_LTLIBRARIES = libdirectfbwm_default.la


libdirectfbwm_default_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfbwm_default_la_SOURCES = \
	default.c

libdirectfbwm_default_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
