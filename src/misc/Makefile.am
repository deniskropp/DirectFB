## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DMODULEDIR=\"@MODULEDIR@\"	\
	-DSOPATH=\"@SOPATH@\"           \
	-DSYSCONFDIR=\"@sysconfdir@\"

internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
	conf.h			\
	gfx_util.h		\
	util.h


noinst_LTLIBRARIES = libdirectfb_misc.la


if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_SOURCESs = 
else
NON_PURE_VOODOO_SOURCESs = \
	gfx_util.c
endif

libdirectfb_misc_la_SOURCES =	\
	$(NON_PURE_VOODOO_SOURCESs)	\
	conf.c			\
	dither.h		\
	dither565.h		\
	util.c
