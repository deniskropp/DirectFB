## Makefile.am for DirectFB/inputdrivers/ucb1x00_ts

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

input_LTLIBRARIES = libdirectfb_ucb1x00_ts.la

inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_ucb1x00_ts_la_SOURCES =	\
	ucb1x00_ts.c

libdirectfb_ucb1x00_ts_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
