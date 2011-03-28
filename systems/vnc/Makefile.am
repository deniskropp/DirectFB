## Makefile.am for DirectFB/systems/vnc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(VNC_CFLAGS)

systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_vnc.o
endif

systems_LTLIBRARIES = libdirectfb_vnc.la

libdirectfb_vnc_la_LDFLAGS = \
	$(VNC_LIBS)	\
	-avoid-version	\
	-module

libdirectfb_vnc_la_SOURCES = \
	primary.c	\
	primary.h	\
	vncinput.c	\
	vnc.c		\
	vnc.h

libdirectfb_vnc_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

