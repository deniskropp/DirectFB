## Makefile.am for DirectFB/interfaces/IWater

iwaterdir = $(MODULEDIR)/interfaces/IWater


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

iwater_LTLIBRARIES = \
	libiwater_default.la

if BUILD_STATIC
iwater_DATA = $(iwater_LTLIBRARIES:.la=.o)
endif


libiwater_default_la_SOURCES = \
	elements.c		\
	elements.h		\
	iwater_default.c	\
	iwater_default.h	\
	transform.c		\
	transform.h		\
	util.c			\
	util.h

libiwater_default_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	-lm

libiwater_default_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make
