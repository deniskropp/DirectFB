## Makefile.am for DirectFB/inputdrivers/linux_input

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

linux_input_LTLIBRARIES = libdirectfb_linux_input.la

if BUILD_STATIC
#linux_input_DATA = $(linux_input_LTLIBRARIES:.la=.o)
endif

linux_inputdir = $(MODULEDIR)/inputdrivers

libdirectfb_linux_input_la_SOURCES = \
	input_fake.h		\
	linux_input.c

libdirectfb_linux_input_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)


#include $(top_srcdir)/rules/libobject.make

