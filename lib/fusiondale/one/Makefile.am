## Makefile.am for lib/fusiondale/one

INTERFACES_DIR = $(MODULEDIR)/interfaces

ifusiondaledir            = $(INTERFACES_DIR)/IFusionDale
icomadir                  = $(INTERFACES_DIR)/IComa
icomacomponentdir         = $(INTERFACES_DIR)/IComaComponent
ifusiondalemessengerdir   = $(INTERFACES_DIR)/IFusionDaleMessenger

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/lib/fusiondale	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/fusiondale \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"


ifusiondale_LTLIBRARIES = \
	libifusiondale_one.la

icoma_LTLIBRARIES = \
	libicoma_one.la

icomacomponent_LTLIBRARIES = \
	libicomacomponent_one.la

ifusiondalemessenger_LTLIBRARIES = \
	libifusiondalemessenger_one.la

if BUILD_STATIC
ifusiondale_DATA          = libifusiondale_one.o
icoma_DATA                = libicoma_one.o
icomacomponent_DATA       = libicomacomponent_one.o
ifusiondalemessenger_DATA = libifusiondalemessenger_one.o
endif


libifusiondale_one_la_SOURCES = ifusiondale_one.c ifusiondale_one.h
libifusiondale_one_la_LDFLAGS = -avoid-version -module
libifusiondale_one_la_LIBADD  = $(top_builddir)/lib/One/libone.la $(top_builddir)/lib/fusiondale/libfusiondale.la

libicoma_one_la_SOURCES = icoma_one.c icoma_one.h
libicoma_one_la_LDFLAGS = -avoid-version -module
libicoma_one_la_LIBADD  = $(top_builddir)/lib/One/libone.la $(top_builddir)/lib/fusiondale/libfusiondale.la

libicomacomponent_one_la_SOURCES = icomacomponent_one.c icomacomponent_one.h
libicomacomponent_one_la_LDFLAGS = -avoid-version -module
libicomacomponent_one_la_LIBADD  = $(top_builddir)/lib/One/libone.la $(top_builddir)/lib/fusiondale/libfusiondale.la

libifusiondalemessenger_one_la_SOURCES = ifusiondalemessenger_one.c ifusiondalemessenger_one.h
libifusiondalemessenger_one_la_LDFLAGS = -avoid-version -module 
libifusiondalemessenger_one_la_LIBADD  = $(top_builddir)/lib/One/libone.la $(top_builddir)/lib/fusiondale/libfusiondale.la

include $(top_srcdir)/rules/libobject.make
