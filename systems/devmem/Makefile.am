## Makefile.am for DirectFB/systems/devmem

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src


internalincludedir = $(INTERNALINCLUDEDIR)/devmem

internalinclude_HEADERS = \
	devmem.h		\
	surfacemanager.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_devmem.o
endif
systems_LTLIBRARIES = libdirectfb_devmem.la

libdirectfb_devmem_la_LDFLAGS = \
	-avoid-version	\
	-module

libdirectfb_devmem_la_SOURCES = \
	devmem.c		\
	devmem_surface_pool.c	\
	surfacemanager.c

libdirectfb_devmem_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
