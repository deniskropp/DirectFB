## Makefile.am for DirectFB/src/core

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
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
	modules.h		\
	palette.h		\
	screen.h		\
	screens.h		\
	sig.h			\
	state.h			\
	surfaces.h		\
	surfacemanager.h	\
	system.h		\
	thread.h		\
	windows.h		\
	windowstack.h


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
	modules.c		\
	palette.c		\
	screen.c		\
	screens.c		\
	screens_internal.h	\
	sig.c			\
	state.c			\
	surfaces.c		\
	surfacemanager.c	\
	system.c		\
	thread.c		\
	window_display.c	\
	windows.c		\
	windows_internal.h	\
	windowstack.c
