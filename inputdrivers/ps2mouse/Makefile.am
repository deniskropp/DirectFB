## Makefile.am for DirectFB/inputdrivers/ps2mouse

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

ps2mouse_LTLIBRARIES = libdirectfb_ps2mouse.la

ps2mousedir = $(MODULEDIR)/inputdrivers

libdirectfb_ps2mouse_la_SOURCES =	\
	ps2mouse.c

libdirectfb_ps2mouse_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
