# Makefile.am for DirectFB/lib/egl/EGLCoreSDL

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/lib/egl	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/egl		\
	-I$(top_srcdir)/src		\
	$(SDL_CFLAGS)



dfbegl_coredir = $(MODULEDIR)/dfbegl_core

if BUILD_STATIC
dfbegl_core_DATA = libdfbegl_core_sdl.o
endif

dfbegl_core_LTLIBRARIES = libdfbegl_core_sdl.la


libdfbegl_core_sdl_la_LDFLAGS = \
	-avoid-version	\
	-module

libdfbegl_core_sdl_la_SOURCES = \
	EGLDisplaySDL.cpp		\
	EGLDisplaySDL.h

libdfbegl_core_sdl_la_LIBADD = \
	$(DFB_BASE_LIBS)	\
	$(libdfbegl)		\
	$(SDL_LIBS)


include $(top_srcdir)/rules/libobject.make
include $(top_srcdir)/rules/libs_deps.make
