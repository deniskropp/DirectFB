## Makefile.am for DirectFB/interfaces/ICoreResourceManager

icoreresourcemanagerdir = $(MODULEDIR)/interfaces/ICoreResourceManager


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

icoreresourcemanager_LTLIBRARIES = \
	libicoreresourcemanager_test.la

if BUILD_STATIC
icoreresourcemanager_DATA = $(icoreresourcemanager_LTLIBRARIES:.la=.o)
endif


libicoreresourcemanager_test_la_SOURCES = \
	icoreresourcemanager_test.c		\
	icoreresourcemanager_test.h		\
	icoreresourcemanager_test_client.c	\
	icoreresourcemanager_test_client.h

libicoreresourcemanager_test_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libicoreresourcemanager_test_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make

