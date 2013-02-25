## Makefile.am for DirectFB/src/misc


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SUBDIRS = 
else
NON_PURE_VOODOO_SUBDIRS = \
	generic
endif

SUBDIRS = \
	$(NON_PURE_VOODOO_SUBDIRS)


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/src		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src


internalincludedir = $(INTERNALINCLUDEDIR)/gfx

internalinclude_HEADERS = \
	clip.h			\
	convert.h		\
	util.h


noinst_LTLIBRARIES = libdirectfb_gfx.la


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SOURCES = 
else
NON_PURE_VOODOO_SOURCES = \
	clip.c			\
	util.cpp
endif

libdirectfb_gfx_la_SOURCES = \
	$(NON_PURE_VOODOO_SOURCES)	\
	convert.c


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_LIBS = 
else
NON_PURE_VOODOO_LIBS = \
	generic/libdirectfb_generic.la
endif

libdirectfb_gfx_la_LIBADD = \
	$(NON_PURE_VOODOO_LIBS)
