## Makefile.am for DirectFB/wm/unique/devices

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/wm

AM_CPPFLAGS = \
	-DMODULEDIR=\"@MODULEDIR@\"	\
	-DSOPATH=\"@SOPATH@\"


noinst_LTLIBRARIES = libunique_devices.la

libunique_devices_la_SOURCES = \
	pointer.c	\
	wheel.c		\
	keyboard.c
