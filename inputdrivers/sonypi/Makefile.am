## Makefile.am for DirectFB/inputdrivers/sonypi

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

sonypi_LTLIBRARIES = libdirectfb_sonypi.la

if BUILD_STATIC
sonypi_DATA = $(sonypi_LTLIBRARIES:.la=.o)
endif

sonypidir = $(MODULEDIR)/inputdrivers

libdirectfb_sonypi_la_SOURCES =	\
	sonypi.c

libdirectfb_sonypi_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)


include $(top_srcdir)/rules/libobject.make
	
