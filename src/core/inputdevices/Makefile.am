## Makefile.am for DirectFB/src/core/inputdevices

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

noinst_LTLIBRARIES = libdirectfb_inputdevices.la

libdirectfb_inputdevices_la_SOURCES = \
	keyboard.c	\
	keyboard.h	\
	joystick.c	\
	joystick.h	\
	ps2mouse.c	\
	ps2mouse.h

libdirectfb_inputdevices_la_LIBADD = $(THREAD_LIBS)
