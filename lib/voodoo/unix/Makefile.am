## Makefile.am for DirectFB/lib/voodoo/unix

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"

noinst_LTLIBRARIES = libvoodoo_unix.la

libvoodoo_unix_la_SOURCES = \
	interfaces_unix.c	\
	link_unix.c
