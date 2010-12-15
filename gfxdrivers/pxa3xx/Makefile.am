## Makefile.am for DirectFB/gfxdrivers/pxa3xx

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems



bin_PROGRAMS = pxa3xx_dump

pxa3xx_dump_SOURCES = pxa3xx_dump.c
pxa3xx_dump_LDADD   = $(top_builddir)/lib/direct/libdirect.la


pxa3xx_LTLIBRARIES = libdirectfb_pxa3xx.la

if BUILD_STATIC
pxa3xx_DATA = $(pxa3xx_LTLIBRARIES:.la=.o)
endif

pxa3xxdir = $(MODULEDIR)/gfxdrivers

libdirectfb_pxa3xx_la_SOURCES =	\
	pxa3xx.c		\
	pxa3xx.h		\
	pxa3xx_blt.c		\
	pxa3xx_blt.h		\
	pxa3xx_regs.h		\
	pxa3xx_types.h		\
	pxa3xx-gcu.h

libdirectfb_pxa3xx_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_pxa3xx_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

