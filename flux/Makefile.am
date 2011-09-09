## Makefile.am for DirectFB/flux

CXX=g++

INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib

AM_CPPFLAGS = -DDATADIR=\"@DATADIR@\"


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_bin_PROGS = 
else
NON_PURE_VOODOO_bin_PROGS = \
	fluxcomp
endif

bin_PROGRAMS = \
	$(NON_PURE_VOODOO_bin_PROGS)


libdirect = ../lib/direct/libdirect.la


fluxcomp_SOURCES = fluxcomp.cpp
fluxcomp_LDADD   = $(libdirect) $(OSX_LIBS)
