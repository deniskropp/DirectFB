## Makefile.am for DirectFB/inputdrivers/joystick

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

joystick_LTLIBRARIES = libdirectfb_joystick.la

if BUILD_STATIC
#joystick_DATA = $(joystick_LTLIBRARIES:.la=.o)
endif

joystickdir = $(MODULEDIR)/inputdrivers

libdirectfb_joystick_la_SOURCES =	\
	joystick.c

libdirectfb_joystick_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
