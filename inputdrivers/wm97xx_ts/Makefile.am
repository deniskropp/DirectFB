## Makefile.am for DirectFB/inputdrivers/wm97xx

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

input_LTLIBRARIES = libdirectfb_wm97xx_ts.la

inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_wm97xx_ts_la_SOURCES =	\
	wm97xx_ts.c

libdirectfb_wm97xx_ts_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
