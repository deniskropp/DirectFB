## Makefile.am for DirectFB/src/core

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"		\
	-DSOPATH=\"@SOPATH@\"		\
	-DMODULEDIR=\"${RUNTIME_SYSROOT}@MODULEDIR@\"	\
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\""


internalincludedir = $(INTERNALINCLUDEDIR)/core

internalinclude_HEADERS = \
	CoreDFB.h		\
	CoreDFB_includes.h	\
	CoreGraphicsState.h	\
	CoreGraphicsState_includes.h	\
	CoreGraphicsStateClient.h	\
	CoreLayer.h		\
	CoreLayer_includes.h	\
	CoreLayerContext.h	\
	CoreLayerContext_includes.h	\
	CoreLayerRegion.h	\
	CoreLayerRegion_includes.h	\
	CorePalette.h		\
	CorePalette_includes.h	\
	CoreSurface.h		\
	CoreSurface_includes.h	\
	CoreWindow.h		\
	CoreWindow_includes.h	\
	CoreWindowStack.h		\
	CoreWindowStack_includes.h	\
	Interface.h		\
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
	layers_internal.h	\
	palette.h		\
	screen.h		\
	screens.h		\
	screens_internal.h	\
	state.h			\
	surface.h		\
	surface_buffer.h	\
	surface_pool.h		\
	surface_pool_bridge.h	\
	system.h		\
	windows.h		\
	windows_internal.h	\
	windowstack.h		\
	wm.h			\
	wm_module.h


noinst_LTLIBRARIES = libdirectfb_core.la


libdirectfb_core_la_SOURCES = \
	CoreDFB.cpp		\
	CoreDFB_real.cpp	\
	CoreGraphicsState.cpp	\
	CoreGraphicsState_real.cpp	\
	CoreGraphicsStateClient.c		\
	CoreLayer.cpp		\
	CoreLayer_real.cpp	\
	CoreLayerContext.cpp	\
	CoreLayerContext_real.cpp	\
	CoreLayerRegion.cpp	\
	CoreLayerRegion_real.cpp	\
	CorePalette.cpp		\
	CorePalette_real.cpp	\
	CoreSurface.cpp		\
	CoreSurface_real.cpp	\
	CoreWindow.cpp		\
	CoreWindow_real.cpp	\
	CoreWindowStack.cpp		\
	CoreWindowStack_real.cpp	\
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
	local_surface_pool.c	\
	palette.c		\
	prealloc_surface_pool.c	\
	screen.c		\
	screens.c		\
	shared_surface_pool.c	\
	state.c			\
	surface.c		\
	surface_buffer.c	\
	surface_core.c		\
	surface_pool.c		\
	surface_pool_bridge.c	\
	system.c		\
	windows.c		\
	windowstack.c		\
	wm.c

distclean-local:
	rm -f CoreDFB.cpp
	rm -f CoreDFB.h
	rm -f CoreGraphicsState.cpp
	rm -f CoreGraphicsState.h
	rm -f CoreLayerContext.cpp
	rm -f CoreLayerContext.h
	rm -f CoreLayer.cpp
	rm -f CoreLayer.h
	rm -f CoreLayerRegion.cpp
	rm -f CoreLayerRegion.h
	rm -f CorePalette.cpp
	rm -f CorePalette.h
	rm -f CoreSurface.cpp
	rm -f CoreSurface.h
	rm -f CoreWindow.cpp
	rm -f CoreWindow.h
	rm -f CoreWindowStack.cpp
	rm -f CoreWindowStack.h

include $(top_srcdir)/rules/flux_comp.make
