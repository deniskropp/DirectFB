## Makefile.am for DirectFB/inputdrivers/dreamboxremote

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

dreamboxremote_LTLIBRARIES = libdirectfb_dreamboxremote.la

if BUILD_STATIC
dreamboxremote_DATA = $(dreamboxremote_LTLIBRARIES:.la=.o)
endif

dreamboxremotedir = $(MODULEDIR)/inputdrivers

libdirectfb_dreamboxremote_la_SOURCES =	\
	dreamboxremote.c

libdirectfb_dreamboxremote_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
