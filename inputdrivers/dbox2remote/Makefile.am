## Makefile.am for DirectFB/inputdrivers/dbox2remote

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

dbox2remote_LTLIBRARIES = libdirectfb_dbox2remote.la

if BUILD_STATIC
dbox2remote_DATA = $(dbox2remote_LTLIBRARIES:.la=.o)
endif

dbox2remotedir = $(MODULEDIR)/inputdrivers

libdirectfb_dbox2remote_la_SOURCES =	\
	dbox2remote.c

libdirectfb_dbox2remote_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
