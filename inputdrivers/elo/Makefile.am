## Makefile.am for DirectFB/inputdrivers/elo

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

input_LTLIBRARIES = libdirectfb_elo.la

inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_elo_la_SOURCES =	\
	elo.c

libdirectfb_elo_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
