## Makefile.am for DirectFB/src/core/gfxcards/vmware

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems

vmware_LTLIBRARIES = libdirectfb_vmware.la

if BUILD_STATIC
vmware_DATA = $(vmware_LTLIBRARIES:.la=.o)
endif

vmwaredir = $(MODULEDIR)/gfxdrivers

libdirectfb_vmware_la_SOURCES =	\
	vmware_2d.c		\
	vmware_2d.h		\
	vmware_gfxdriver.c	\
	vmware_gfxdriver.h

libdirectfb_vmware_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS) -lm

libdirectfb_vmware_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
