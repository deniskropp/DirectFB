## Makefile.am for DirectFB/src/misc

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS)

EXTRA_DIST = \
	ppcasm_memcpy.S			\
	ppcasm_memcpy_cachable.S	\
	ppcasm_memcpy.h			\
	ppc_asm.h

if BUILDPPCASM
if HAVE_LINUX
ppcasm_sources = ppcasm_memcpy.S \
                 ppcasm_memcpy_cachable.S
else
ppcasm_sources = ppcasm_memcpy.S
endif

ppcasm_headers = ppcasm_memcpy.h  \
                 ppc_asm.h
endif

internalincludedir = $(INTERNALINCLUDEDIR)/misc

internalinclude_HEADERS = \
        $(ppcasm_headers)	\
	conf.h			\
	cpu_accel.h		\
	gfx_util.h		\
	mem.h			\
	memcpy.h		\
	tree.h			\
	utf8.h			\
	util.h


noinst_LTLIBRARIES = libdirectfb_misc.la

libdirectfb_misc_la_SOURCES =	\
	conf.c			\
	colorhash.c		\
	colorhash.h		\
	cpu_accel.c		\
	gfx_util.c		\
	mem.c			\
	memcpy.c		\
	tree.c			\
	utf8.c			\
        $(ppcasm_sources)	\
	util.c
