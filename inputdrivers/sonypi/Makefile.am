## Makefile.am for DirectFB/inputdrivers/sonypi

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

sonypi_LTLIBRARIES = libdirectfb_sonypi.la

sonypidir = $(MODULEDIR)/inputdrivers

libdirectfb_sonypi_la_SOURCES =	\
	sonypi.c

libdirectfb_sonypi_la_LDFLAGS = \
        -export-dynamic		\
	-avoid-version		\
        $(DFB_LDFLAGS)
