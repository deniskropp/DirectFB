## Makefile.am for DirectFB/inputdrivers/microtouch

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

input_LTLIBRARIES = libdirectfb_mutouch.la

inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_mutouch_la_SOURCES = mutouch.c

libdirectfb_mutouch_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
