## Makefile.am for DirectFB/wm/unique/classes

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/wm

AM_CPPFLAGS = \
	-DMODULEDIR=\"@MODULEDIR@\"	\
	-DSOPATH=\"@SOPATH@\"


noinst_LTLIBRARIES = libunique_classes.la

libunique_classes_la_SOURCES = \
	foo.c		\
	frame.c		\
	root.c		\
	window.c
