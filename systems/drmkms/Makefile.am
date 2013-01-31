## Makefile.am for DirectFB/systems/drmkms

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(DRMKMS_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/drmkms_system

internalinclude_HEADERS = \
	drmkms_system.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_drmkms_system.o
endif
systems_LTLIBRARIES = libdirectfb_drmkms_system.la

libdirectfb_drmkms_system_la_LDFLAGS = \
	-avoid-version	\
	-module \
	$(DRMKMS_LIBS)

libdirectfb_drmkms_system_la_SOURCES = \
	drmkms_layer.c		\
	drmkms_screen.c		\
	drmkms_system.c		\
	drmkms_surface_pool.c     \
	vt.c			\
	vt.h

libdirectfb_drmkms_system_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la

include $(top_srcdir)/rules/libobject.make
