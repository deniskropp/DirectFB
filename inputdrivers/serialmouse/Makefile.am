## Makefile.am for DirectFB/inputdrivers/serialmouse

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

serialmouse_LTLIBRARIES = libdirectfb_serialmouse.la

if BUILD_STATIC
serialmouse_DATA = $(serialmouse_LTLIBRARIES:.la=.o)
endif

serialmousedir = $(MODULEDIR)/inputdrivers

libdirectfb_serialmouse_la_SOURCES = serialmouse.c

libdirectfb_serialmouse_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
