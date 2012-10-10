## Makefile.am for DirectFB/inputdrivers/divine

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

divine_LTLIBRARIES = libdirectfb_divine.la

if BUILD_STATIC
divine_DATA = $(divine_LTLIBRARIES:.la=.o)
endif

divinedir = $(MODULEDIR)/inputdrivers

libdirectfb_divine_la_SOURCES =	\
	divine.c

libdirectfb_divine_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_divine_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
