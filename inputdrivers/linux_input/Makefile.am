## Makefile.am for DirectFB/inputdrivers/linux_input

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

linux_input_LTLIBRARIES = libdirectfb_linux_input.la

linux_inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_linux_input_la_SOURCES = \
	input_fake.h		\
	linux_input.c

libdirectfb_linux_input_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
