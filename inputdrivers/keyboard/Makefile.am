## Makefile.am for DirectFB/inputdrivers/keyboard

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

keyboard_LTLIBRARIES = libdirectfb_keyboard.la

keyboarddir = $(MODULEDIR)/inputdrivers

libdirectfb_keyboard_la_SOURCES =	\
	keyboard.c

libdirectfb_keyboard_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
