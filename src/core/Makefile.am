## Makefile.am for DirectFB/src/core

if SDL_CORE
SDL_DIR = sdl
SDL_LIB = sdl/libdirectfb_core_sdl.la
else
SDL_DIR =
SDL_LIB =
endif

if FBDEV_CORE
FBDEV_DIR = fbdev
FBDEV_LIB = fbdev/libdirectfb_core_fbdev.la
else
FBDEV_DIR =
FBDEV_LIB =
endif

SUBDIRS = fusion $(FBDEV_DIR) $(SDL_DIR)

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""


internalincludedir = $(INTERNALINCLUDEDIR)/core

internalinclude_HEADERS = \
	coredefs.h		\
	coretypes.h		\
	core_system.h		\
	core.h			\
	fonts.h			\
	gfxcard.h		\
	graphics_driver.h	\
	input.h			\
	input_driver.h		\
	layers.h		\
	modules.h		\
	palette.h		\
	sig.h			\
	state.h			\
	surfaces.h		\
	surfacemanager.h	\
	system.h		\
	thread.h		\
	windows.h


noinst_LTLIBRARIES = libdirectfb_core.la


libdirectfb_core_la_LIBADD = \
	fusion/libdirectfb_core_fusion.la \
	$(FBDEV_LIB) \
	$(SDL_LIB)

libdirectfb_core_la_SOURCES = \
	core.c			\
	fonts.c			\
	gfxcard.c		\
	input.c			\
	layers.c		\
	modules.c		\
	palette.c		\
	sig.c			\
	state.c			\
	surfaces.c		\
	surfacemanager.c	\
	system.c		\
	thread.c		\
	windows.c
