## Makefile.am for DirectFB/inputdrivers/h3600_ts

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

input_LTLIBRARIES = libdirectfb_h3600_ts.la

inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_h3600_ts_la_SOURCES =	\
	h3600_ts.c

libdirectfb_h3600_ts_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
