## Makefile.am for DirectFB/systems/fbdev

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -D_XOPEN_SOURCE=500


internalincludedir = $(INTERNALINCLUDEDIR)/fbdev

internalinclude_HEADERS = \
	agp.h			\
	fb.h			\
	fbdev.h			\
	surfacemanager.h	\
	vt.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_fbdev.o
endif
systems_LTLIBRARIES = libdirectfb_fbdev.la

libdirectfb_fbdev_la_LDFLAGS = \
	-avoid-version	\
	-module	\
	$(SYSFS_LIBS)

libdirectfb_fbdev_la_SOURCES = \
	agp.c			\
	fbdev.c			\
	fbdev_surface_pool.c	\
	surfacemanager.c	\
	vt.c

libdirectfb_fbdev_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
