## Makefile.am for DirectFB/inputdrivers/lirc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

lirc_LTLIBRARIES = libdirectfb_lirc.la

lircdir = $(MODULEDIR)/inputdrivers

libdirectfb_lirc_la_SOURCES =	\
	lirc.c

libdirectfb_lirc_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
