## Makefile.am for SaWMan/wm/sawman

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/src		\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/sawman	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/sawman


wmdir = $(MODULEDIR)/wm

if BUILD_STATIC
wm_DATA = libdirectfbwm_sawman.o
endif

wm_LTLIBRARIES = libdirectfbwm_sawman.la


libdirectfbwm_sawman_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfbwm_sawman_la_SOURCES = \
	sawman_wm.c

libdirectfbwm_sawman_la_LIBADD = \
	$(top_builddir)/lib/sawman/libsawman.la	\
	$(DIRECTFB_BASE_LIBS)


include $(top_srcdir)/rules/libobject.make
include $(top_srcdir)/rules/libs_deps.make
