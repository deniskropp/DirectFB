## Makefile.am for DirectFB/inputdrivers/joystick

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

joystick_LTLIBRARIES = libdirectfb_joystick.la

joystickdir = $(MODULEDIR)/inputdrivers

libdirectfb_joystick_la_SOURCES =	\
	joystick.c

libdirectfb_joystick_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
