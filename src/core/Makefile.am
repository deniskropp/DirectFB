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
	CoreInputDevice.h		\
	CoreInputDevice_includes.h	\
	CoreLayer.h		\
	CoreLayer_includes.h	\
	CoreLayerContext.h	\
	CoreLayerContext_includes.h	\
	CoreLayerRegion.h	\
	CoreLayerRegion_includes.h	\
	CorePalette.h		\
	CorePalette_includes.h	\
	CoreScreen.h		\
	CoreScreen_includes.h	\
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
	graphics_state.h	\
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
	CoreDFB.c		\
	CoreDFB_real.c		\
	CoreGraphicsState.c	\
	CoreGraphicsState_real.c	\
	CoreGraphicsStateClient.c		\
	CoreInputDevice.c		\
	CoreInputDevice_real.c	\
	CoreLayer.c		\
	CoreLayer_real.c	\
	CoreLayerContext.c	\
	CoreLayerContext_real.c	\
	CoreLayerRegion.c	\
	CoreLayerRegion_real.c	\
	CorePalette.c		\
	CorePalette_real.c	\
	CoreScreen.c		\
	CoreScreen_real.c	\
	CoreSurface.c		\
	CoreSurface_real.c	\
	CoreWindow.c		\
	CoreWindow_real.c	\
	CoreWindowStack.c		\
	CoreWindowStack_real.c	\
	clipboard.c		\
	colorhash.c		\
	core.c			\
	core_parts.c		\
	fonts.c			\
	gfxcard.c		\
	graphics_state.c	\
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
	shared_secure_surface_pool.c	\
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
	rm -f CoreDFB.c
	rm -f CoreDFB.h
	rm -f CoreGraphicsState.c
	rm -f CoreGraphicsState.h
	rm -f CoreInputDevice.c
	rm -f CoreInputDevice.h
	rm -f CoreLayerContext.c
	rm -f CoreLayerContext.h
	rm -f CoreLayer.c
	rm -f CoreLayer.h
	rm -f CoreLayerRegion.c
	rm -f CoreLayerRegion.h
	rm -f CorePalette.c
	rm -f CorePalette.h
	rm -f CoreScreen.c
	rm -f CoreScreen.h
	rm -f CoreSurface.c
	rm -f CoreSurface.h
	rm -f CoreWindow.c
	rm -f CoreWindow.h
	rm -f CoreWindowStack.c
	rm -f CoreWindowStack.h

# quirks for generated headers
CoreGraphicsStateClient.c: CoreGraphicsState.h CoreDFB.h
core.c: CoreDFB.h
surface.c: CoreDFB.h
graphics_state.c: CoreGraphicsState.h
input.c: CoreInputDevice.h
layer_context.c: CoreLayerContext.h
layers.c: CoreLayer.h
layer_region.c: CoreLayerRegion.h
palette.c: CorePalette.h
screens.c: CoreScreen.h
surface.c: CoreSurface.h
surface_buffer.c: CoreSurface.h
windows.c: CoreWindow.h
windowstack.c: CoreWindowStack.h

CoreDFB_real.c: CoreGraphicsState.h CoreDFB.h
CoreGraphicsState_real.c: CoreGraphicsState.h
CoreInputDevice_real.c: CoreInputDevice.h
CoreLayerContext_real.c: CoreLayerContext.h
CoreLayer_real.c: CoreLayer.h
CoreLayerRegion_real.c: CoreLayerRegion.h
CorePalette_real.c: CorePalette.h
CoreScreen_real.c: CoreScreen.h
CoreSurface_real.c: CoreSurface.h
CoreWindow_real.c: CoreWindow.h
CoreWindowStack_real.c: CoreWindowStack.h


include $(top_srcdir)/rules/flux_comp.make
