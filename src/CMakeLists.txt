include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/core")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/media")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}/core")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}/media")

add_custom_command(
        OUTPUT  core/core_strings.h
	COMMAND mkdir -p core
        COMMAND echo "\\#ifndef __CORE__CORE_STRINGS_H__" >  core/core_strings.h
        COMMAND echo "\\#define __CORE__CORE_STRINGS_H__" >> core/core_strings.h
        COMMAND echo "\\#include \\<core/surface.h\\>"    >> core/core_strings.h
        COMMAND ${MKNAMES} CoreSurfaceTypeFlags CSTF NONE CoreSurfaceTypeFlags flag ${CMAKE_CURRENT_SOURCE_DIR}/core/surface.h | grep -v CSTF_ALL >> core/core_strings.h
        COMMAND echo "\\#endif"    >> core/core_strings.h
	DEPENDS core/surface.h
)

FLUX_FILE (core CoreDFB)
FLUX_FILE (core CoreSlave)
FLUX_FILE (core CoreGraphicsState)
FLUX_FILE (core CoreInputDevice)
FLUX_FILE (core CoreLayer)
FLUX_FILE (core CoreLayerContext)
FLUX_FILE (core CoreLayerRegion)
FLUX_FILE (core CorePalette)
FLUX_FILE (core CoreScreen)
FLUX_FILE (core CoreSurface)
FLUX_FILE (core CoreSurfaceClient)
FLUX_FILE (core CoreWindow)
FLUX_FILE (core CoreWindowStack)
FLUX_FILE (media DataBuffer)
FLUX_FILE (media ImageProvider)


set (LIBDIRECTFB_SRC
	directfb.c
	directfb_result.c
	dummy.cpp
	init.c

	gfx/convert.c

	input/idirectfbinputbuffer.c

	media/idirectfbdatabuffer.c
	media/idirectfbdatabuffer_file.c
	media/idirectfbdatabuffer_memory.c
	media/idirectfbdatabuffer_streamed.c

	misc/conf.c
	misc/util.c
)

set (LIBDIRECTFB_NON_PURE_VOODOO_SRC
	core/core_strings.h

	idirectfb.c

	display/idirectfbpalette.c
	display/idirectfbsurface.c
	display/idirectfbsurface_layer.c
	display/idirectfbsurface_window.c
	display/idirectfbdisplaylayer.c
	display/idirectfbscreen.c
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreDFB.cpp
	core/CoreDFB_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreSlave.cpp
	core/CoreSlave_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreGraphicsState.cpp
	core/CoreGraphicsState_real.cpp
	core/CoreGraphicsStateClient.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreInputDevice.cpp
	core/CoreInputDevice_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreLayer.cpp
	core/CoreLayer_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreLayerContext.cpp
	core/CoreLayerContext_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreLayerRegion.cpp
	core/CoreLayerRegion_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CorePalette.cpp
	core/CorePalette_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreScreen.cpp
	core/CoreScreen_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreSurface.cpp
	core/CoreSurface_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreSurfaceClient.cpp
	core/CoreSurfaceClient_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreWindow.cpp
	core/CoreWindow_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/core/CoreWindowStack.cpp
	core/CoreWindowStack_real.cpp
	core/Debug.cpp
	core/DisplayTask.cpp
	core/Interface.cpp
	core/Renderer.cpp
	core/SurfaceTask.cpp
	core/Task.cpp
	core/TaskManager.cpp
	core/TaskThreadsQ.cpp
	core/Util.cpp
	core/clipboard.c
	core/colorhash.c
	core/core.c
	core/core_parts.c
	core/fonts.c
	core/gfxcard.c
	core/graphics_state.c
	core/input.c
	core/input_hub.c
	core/layer_context.c
	core/layer_control.c
	core/layer_region.c
	core/layers.c
	core/local_surface_pool.c
	core/palette.c
	core/prealloc_surface_pool.c
	core/prealloc_surface_pool_bridge.c
	core/screen.c
	core/screens.c
	core/shared_secure_surface_pool.c
	core/shared_surface_pool.c
	core/state.c
	core/surface.c
	core/surface_allocation.cpp
	core/surface_buffer.c
	core/surface_client.c
	core/surface_core.c
	core/surface_pool.c
	core/surface_pool_bridge.c
	core/system.c
	core/windows.c
	core/windowstack.c
	core/wm.c

	gfx/clip.c
	gfx/util.cpp

	gfx/generic/GenefxEngine.cpp
	gfx/generic/generic.c
	gfx/generic/generic_fill_rectangle.c
	gfx/generic/generic_draw_line.c
	gfx/generic/generic_blit.c
	gfx/generic/generic_stretch_blit.c
	gfx/generic/generic_texture_triangles.c
	gfx/generic/generic_util.c

	input/idirectfbinputdevice.c

	media/idirectfbdatabuffer_client.c
	media/idirectfbfont.c
	media/idirectfbimageprovider.c
	media/idirectfbimageprovider_client.c
	media/idirectfbvideoprovider.c
	${CMAKE_CURRENT_BINARY_DIR}/media/DataBuffer.cpp
	media/DataBuffer_real.cpp
	${CMAKE_CURRENT_BINARY_DIR}/media/ImageProvider.cpp
	media/ImageProvider_real.cpp

	misc/gfx_util.c

	windows/idirectfbwindow.c
)

add_library (directfb SHARED ${LIBDIRECTFB_SRC} ${LIBDIRECTFB_NON_PURE_VOODOO_SRC})



target_link_libraries ( directfb
	direct
	fusion
        pthread
        dl
        rt
)

INSTALL_DIRECTFB_LIB (directfb)

