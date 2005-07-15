## Makefile.am for DirectFB/systems/vnc

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	$(VNC_CFLAGS)

systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_vnc.o
endif

systems_LTLIBRARIES = libdirectfb_vnc.la

inputdriversdir = $(MODULEDIR)/inputdrivers

if BUILD_STATIC
inputdrivers_DATA = libdirectfb_vncinput.o
endif
inputdrivers_LTLIBRARIES = libdirectfb_vncinput.la

libdirectfb_vnc_la_LDFLAGS = \
	$(VNC_LIBS)	\
	-avoid-version	\
	-module

libdirectfb_vnc_la_SOURCES = \
	primary.c	\
	primary.h	\
	vnc.c		\
	vnc.h

libdirectfb_vncinput_la_LDFLAGS = \
	$(VNC_LIBS)	\
	-avoid-version	\
	-module

libdirectfb_vncinput_la_SOURCES = \
	vncinput.c

include $(top_srcdir)/rules/libobject.make

