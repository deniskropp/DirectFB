## Makefile.am for DirectFB/inputdrivers/dbox2remote

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

dbox2remote_LTLIBRARIES = libdirectfb_dbox2remote.la

dbox2remotedir = $(MODULEDIR)/inputdrivers

libdirectfb_dbox2remote_la_SOURCES =	\
	dbox2remote.c

libdirectfb_dbox2remote_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
