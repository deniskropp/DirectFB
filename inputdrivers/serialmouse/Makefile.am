## Makefile.am for DirectFB/inputdrivers/msmouse

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

serialmouse_LTLIBRARIES = libdirectfb_serialmouse.la

serialmousedir = $(LIBDIR)/inputdrivers

libdirectfb_serialmouse_la_SOURCES = serialmouse.c

libdirectfb_serialmouse_la_LDFLAGS = \
        -export-dynamic		\
	-disable-static		\
	-avoid-version		\
        $(DFB_LDFLAGS)

