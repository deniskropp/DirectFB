## Makefile.am for DirectFB/inputdrivers/input_hub

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

input_hub_LTLIBRARIES = libdirectfb_input_hub.la

if BUILD_STATIC
input_hub_DATA = $(input_hub_LTLIBRARIES:.la=.o)
endif

input_hubdir = $(MODULEDIR)/inputdrivers

libdirectfb_input_hub_la_SOURCES = \
	input_hub_driver.c

libdirectfb_input_hub_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_input_hub_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

