## Makefile.am for DirectFB/inputdrivers/tslib

INCLUDES = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src

tslib_LTLIBRARIES = libdirectfb_tslib.la

if BUILD_STATIC
tslib_DATA = $(tslib_LTLIBRARIES:.la=.o)
endif

tslibdir = $(MODULEDIR)/inputdrivers

libdirectfb_tslib_la_SOURCES = \
	tslib.c

libdirectfb_tslib_la_CFLAGS = \
	$(TSLIB_CFLAGS)

libdirectfb_tslib_la_LDFLAGS = \
	-export-dynamic \
	-avoid-version \
	$(DFB_LDFLAGS)

libdirectfb_tslib_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(TSLIB_LIBS)

include $(top_srcdir)/rules/libobject.make
