## Makefile.am for DirectFB/src/core

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""


internalincludedir = $(INTERNALINCLUDEDIR)/core

internalinclude_HEADERS = \
	clipboard.h		\
	colorhash.h		\
	coredefs.h		\
	coretypes.h		\
	core_parts.h		\
	core_system.h		\
	core.h			\
	fonts.h			\
	gfxcard.h		\
	graphics_driver.h	\
	input.h			\
	input_driver.h		\
	layer_context.h		\
	layer_control.h		\
	layer_region.h		\
	layers.h		\
	palette.h		\
	screen.h		\
	screens.h		\
	state.h			\
	surfaces.h		\
	surfacemanager.h	\
	system.h		\
	windows.h		\
	windowstack.h		\
	wm.h			\
	wm_module.h


noinst_LTLIBRARIES = libdirectfb_core.la


libdirectfb_core_la_SOURCES = \
	clipboard.c		\
	colorhash.c		\
	core.c			\
	core_parts.c		\
	fonts.c			\
	gfxcard.c		\
	input.c			\
	layer_context.c		\
	layer_control.c		\
	layer_region.c		\
	layers.c		\
	layers_internal.h	\
	palette.c		\
	screen.c		\
	screens.c		\
	screens_internal.h	\
	state.c			\
	surfaces.c		\
	surfacemanager.c	\
	system.c		\
	window_display.c	\
	windows.c		\
	windows_internal.h	\
	windowstack.c		\
	wm.c
