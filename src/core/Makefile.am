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
	"-DBUILDTIME=\"`date -u "+%Y-%m-%d %H:%M"`\"" \
	-DFLUXED_ARGS_BYTES=@FLUXED_ARGS_BYTES@


MKNAMES = $(top_srcdir)/tools/mknames.sh
INCP = $(srcdir)

DISTCLEANFILES = core_strings.h

core_strings.h: surface.h $(MKNAMES) Makefile
	@echo '#ifndef __CORE__CORE_STRINGS_H__' > $@
	@echo '#define __CORE__CORE_STRINGS_H__' >> $@
	@echo '#include <core/surface.h>' >> $@

	@$(MKNAMES) CoreSurfaceTypeFlags CSTF NONE CoreSurfaceTypeFlags flag $(INCP)/surface.h \
	   | grep -v CSTF_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


internalincludedir = $(INTERNALINCLUDEDIR)/core

internalinclude_HEADERS = \
	CoreDFB_CallMode.h		\
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
	$(builddir)/CoreSurfaceClient.h		\
	CoreSurfaceClient_includes.h	\
	$(builddir)/CoreWindow.h		\
	CoreWindow_includes.h	\
	$(builddir)/CoreWindowStack.h		\
	CoreWindowStack_includes.h	\
	Debug.h			\
	DisplayTask.h		\
	Fifo.h			\
	Interface.h		\
	PacketBuffer.h		\
	Renderer.h		\
	SurfaceTask.h		\
	Task.h			\
	TaskManager.h		\
	TaskThreadsQ.h		\
	Util.h			\
	clipboard.h		\
	colorhash.h		\
	coredefs.h		\
	coretypes.h		\
	core_parts.h		\
	core_strings.h		\
	core_system.h		\
	core.h			\
	fonts.h			\
	gfxcard.h		\
	graphics_driver.h	\
	graphics_state.h	\
	input.h			\
	input_driver.h		\
	input_hub.h		\
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
	surface_allocation.h	\
	surface_buffer.h	\
	surface_client.h	\
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
	$(builddir)/CoreDFB.cpp		\
	CoreDFB_real.cpp	\
	$(builddir)/CoreSlave.cpp		\
	CoreSlave_real.cpp	\
	$(builddir)/CoreGraphicsState.cpp	\
	CoreGraphicsState_real.cpp	\
	CoreGraphicsStateClient.cpp		\
	$(builddir)/CoreInputDevice.cpp		\
	CoreInputDevice_real.cpp	\
	$(builddir)/CoreLayer.cpp		\
	CoreLayer_real.cpp	\
	$(builddir)/CoreLayerContext.cpp	\
	CoreLayerContext_real.cpp	\
	$(builddir)/CoreLayerRegion.cpp	\
	CoreLayerRegion_real.cpp	\
	$(builddir)/CorePalette.cpp		\
	CorePalette_real.cpp	\
	$(builddir)/CoreScreen.cpp		\
	CoreScreen_real.cpp	\
	$(builddir)/CoreSurface.cpp		\
	CoreSurface_real.cpp	\
	$(builddir)/CoreSurfaceClient.cpp		\
	CoreSurfaceClient_real.cpp	\
	$(builddir)/CoreWindow.cpp		\
	CoreWindow_real.cpp	\
	$(builddir)/CoreWindowStack.cpp		\
	CoreWindowStack_real.cpp	\
	Debug.cpp		\
	DisplayTask.cpp		\
	Interface.cpp		\
	Renderer.cpp		\
	SurfaceTask.cpp		\
	Task.cpp		\
	TaskManager.cpp		\
	TaskThreadsQ.cpp	\
	Util.cpp		\
	clipboard.c		\
	colorhash.c		\
	core.c			\
	core_parts.c		\
	fonts.c			\
	gfxcard.c		\
	graphics_state.c	\
	input.c			\
	input_hub.c		\
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
	surface_allocation.cpp	\
	surface_buffer.c	\
	surface_client.c	\
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
	rm -f CoreSlave.cpp
	rm -f CoreSlave.h
	rm -f CoreGraphicsState.cpp
	rm -f CoreGraphicsState.h
	rm -f CoreInputDevice.cpp
	rm -f CoreInputDevice.h
	rm -f CoreLayerContext.cpp
	rm -f CoreLayerContext.h
	rm -f CoreLayer.cpp
	rm -f CoreLayer.h
	rm -f CoreLayerRegion.cpp
	rm -f CoreLayerRegion.h
	rm -f CorePalette.cpp
	rm -f CorePalette.h
	rm -f CoreScreen.cpp
	rm -f CoreScreen.h
	rm -f CoreSurface.cpp
	rm -f CoreSurface.h
	rm -f CoreWindow.cpp
	rm -f CoreWindow.h
	rm -f CoreWindowStack.cpp
	rm -f CoreWindowStack.h

# quirks for generated headers
$(srcdir)/CoreGraphicsStateClient.cpp: CoreGraphicsState.h CoreDFB.h
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

$(srcdir)/CoreDFB_real.cpp: CoreGraphicsState.h CoreDFB.h
$(srcdir)/CoreGraphicsState_real.cpp: CoreGraphicsState.h
$(srcdir)/CoreInputDevice_real.cpp: CoreInputDevice.h
$(srcdir)/CoreLayerContext_real.cpp: CoreLayerContext.h
$(srcdir)/CoreLayer_real.cpp: CoreLayer.h
$(srcdir)/CoreLayerRegion_real.cpp: CoreLayerRegion.h
$(srcdir)/CorePalette_real.cpp: CorePalette.h
$(srcdir)/CoreScreen_real.cpp: CoreScreen.h
$(srcdir)/CoreSlave_real.cpp: CoreSlave.h
$(srcdir)/CoreSurface_real.cpp: CoreSurface.h
$(srcdir)/CoreSurfaceClient_real.cpp: CoreSurfaceClient.h
$(srcdir)/CoreWindow_real.cpp: CoreWindow.h
$(srcdir)/CoreWindowStack_real.cpp: CoreWindowStack.h

$(srcdir)/Debug.cpp: core_strings.h


include $(top_srcdir)/rules/flux_comp.make
