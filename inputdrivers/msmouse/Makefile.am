## Makefile.am for DirectFB/inputdrivers/msmouse

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

msmouse_LTLIBRARIES = libdirectfb_msmouse.la

msmousedir = $(LIBDIR)/inputdrivers

libdirectfb_msmouse_la_SOURCES = msmouse.c

libdirectfb_msmouse_la_LDFLAGS = \
        -export-dynamic		\
	-disable-static		\
	-avoid-version		\
        $(DFB_LDFLAGS)

