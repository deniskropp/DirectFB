## Makefile.am for DirectFB/src/core

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/src		\
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
	$(builddir)/CoreDFB.h		\
	CoreDFB_includes.h	\
	$(builddir)/CoreSlave.h		\
	CoreSlave_includes.h	\
	$(builddir)/CoreGraphicsState.h	\
	CoreGraphicsState_includes.h	\
	CoreGraphicsStateClient.h	\
	$(builddir)/CoreInputDevice.h		\
	CoreInputDevice_includes.h	\
	$(builddir)/CoreLayer.h		\
	CoreLayer_includes.h	\
	$(builddir)/CoreLayerContext.h	\
	CoreLayerContext_includes.h	\
	$(builddir)/CoreLayerRegion.h	\
	CoreLayerRegion_includes.h	\
	$(builddir)/CorePalette.h		\
	CorePalette_includes.h	\
	$(builddir)/CoreScreen.h		\
	CoreScreen_includes.h	\
	$(builddir)/CoreSurface.h		\
	CoreSurface_includes.h	\
	$(builddir)/CoreWindow.h		\
	CoreWindow_includes.h	\
	$(builddir)/CoreWindowStack.h		\
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
	surface_core.h		\
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
	$(builddir)/CoreDFB.c		\
	CoreDFB_real.c	\
	$(builddir)/CoreSlave.c		\
	CoreSlave_real.c	\
	$(builddir)/CoreGraphicsState.c	\
	CoreGraphicsState_real.c	\
	CoreGraphicsStateClient.c		\
	$(builddir)/CoreInputDevice.c		\
	CoreInputDevice_real.c	\
	$(builddir)/CoreLayer.c		\
	CoreLayer_real.c	\
	$(builddir)/CoreLayerContext.c	\
	CoreLayerContext_real.c	\
	$(builddir)/CoreLayerRegion.c	\
	CoreLayerRegion_real.c	\
	$(builddir)/CorePalette.c		\
	CorePalette_real.c	\
	$(builddir)/CoreScreen.c		\
	CoreScreen_real.c	\
	$(builddir)/CoreSurface.c		\
	CoreSurface_real.c	\
	$(builddir)/CoreWindow.c		\
	CoreWindow_real.c	\
	$(builddir)/CoreWindowStack.c		\
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
	prealloc_surface_pool_bridge.c	\
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
	rm -f CoreSlave.c
	rm -f CoreSlave.h
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
$(srcdir)/CoreGraphicsStateClient.c: CoreGraphicsState.h CoreDFB.h
$(srcdir)/core.c: CoreDFB.h CoreSlave.h
$(srcdir)/surface.c: CoreDFB.h
$(srcdir)/graphics_state.c: CoreGraphicsState.h
$(srcdir)/input.c: CoreInputDevice.h
$(srcdir)/layer_context.c: CoreLayerContext.h
$(srcdir)/layers.c: CoreLayer.h
$(srcdir)/layer_region.c: CoreLayerRegion.h
$(srcdir)/palette.c: CorePalette.h
$(srcdir)/screens.c: CoreScreen.h
$(srcdir)/surface.c: CoreSurface.h
$(srcdir)/surface_buffer.c: CoreSurface.h
$(srcdir)/windows.c: CoreWindow.h
$(srcdir)/windowstack.c: CoreWindowStack.h

$(srcdir)/CoreDFB_real.c: CoreGraphicsState.h CoreDFB.h
$(srcdir)/CoreGraphicsState_real.c: CoreGraphicsState.h
$(srcdir)/CoreInputDevice_real.c: CoreInputDevice.h
$(srcdir)/CoreLayerContext_real.c: CoreLayerContext.h
$(srcdir)/CoreLayer_real.c: CoreLayer.h
$(srcdir)/CoreLayerRegion_real.c: CoreLayerRegion.h
$(srcdir)/CorePalette_real.c: CorePalette.h
$(srcdir)/CoreScreen_real.c: CoreScreen.h
$(srcdir)/CoreSurface_real.c: CoreSurface.h
$(srcdir)/CoreWindow_real.c: CoreWindow.h
$(srcdir)/CoreWindowStack_real.c: CoreWindowStack.h


include $(top_srcdir)/rules/flux_comp.make
