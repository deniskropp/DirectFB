## Makefile.am for DirectFB/inputdrivers/serialmouse

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

serialmouse_LTLIBRARIES = libdirectfb_serialmouse.la

serialmousedir = $(MODULEDIR)/inputdrivers

libdirectfb_serialmouse_la_SOURCES = serialmouse.c

libdirectfb_serialmouse_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
