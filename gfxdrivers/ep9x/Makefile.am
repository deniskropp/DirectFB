## Makefile.am for DirectFB/src/core/gfxcards/ep9x

INCLUDES = \
        -I$(top_builddir)/include \
        -I$(top_builddir)/lib   \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/lib     \
        -I$(top_srcdir)/src     \
        -I$(top_srcdir)/systems

ep9xdir = $(MODULEDIR)/gfxdrivers
ep9x_LTLIBRARIES = libdirectfb_ep9x.la

if BUILD_STATIC
ep9x_DATA = $(e9x_LTLIBRARIES:.la=.o)
endif

libdirectfb_ep9x_la_SOURCES =   \
        ep9x.c  \
        ep9x.h 

libdirectfb_ep9x_la_LDFLAGS = \
        -module	                \
        -avoid-version          \
        $(DFB_LDFLAGS)

libdirectfb_ep9x_la_LIBADD = \
        $(top_builddir)/lib/direct/libdirect.la \
        $(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

