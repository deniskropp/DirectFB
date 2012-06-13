#
# Global setup / android specific stuff
LOCAL_PATH    := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_MODULE := directfb


DFB_SOURCE = ../..
KERNEL = $(LOCAL_PATH)/Kernel

LOCAL_CFLAGS = -I$(LOCAL_PATH) -I$(LOCAL_PATH)/.. $(INCLUDES) -I$(LOCAL_PATH)/../freetype2-android/include $(CFLAGS) $(CPPFLAGS) -DANDROID_NDK   -fno-short-enums
LOCAL_LDFLAGS = -lEGL -lGLESv2 -llog -landroid $(LOCAL_PATH)/../freetype2-android/Android/obj/local/x86/libfreetype2-static.a
#LOCAL_LDFLAGS = -lEGL -lGLESv2 -llog -landroid

#
# Version definition
MAJOR   = 1
MINOR   = 6
TINY    = 0
VERSION = $(MAJOR).$(MINOR).$(TINY)

DEBUG=yes

buildtime = $(shell sh -c date -u +%Y-%m-%d %H:%M)

CPPFLAGS +=	\
	-DHAVE_CONFIG_H							\
	-DHAVE_STDBOOL_H						\
	-D_GNU_SOURCE							\
	-D_REENTRANT							\
	-DVERSION=\"$(VERSION)\"					\
        -DBUILDTIME="\"$(buildtime)\"" \
        -DHAVE_SIGNAL_H \
        -DDIRECT_BUILD_NO_PTHREAD_CANCEL=1 \
        -DDIRECT_BUILD_NO_PTHREAD_CONDATTR=1 \
	-DDIRECT_BUILD_NO_SA_SIGINFO=1 \
	-DDIRECT_BUILD_NO_SIGQUEUE=1 \
	-DGLES2_PVR2D \
        -DPTHREADMINIT \
	-DDATADIR=\"/mnt/sdcard/directfb\"	\
	-DFONT=\"/mnt/sdcard/directfb/decker.ttf\"	\
	-DSYSCONFDIR=\"/mnt/sdcard/directfb\"
#
# Debug option
ifeq ($(DEBUG),yes)
  CPPFLAGS += -DENABLE_DEBUG=1
  CFLAGS   += -g3
else
  CPPFLAGS += -DENABLE_DEBUG=0
endif

#
# Trace option
ifeq ($(TRACE),yes)
  CFLAGS   += -finstrument-functions
  CPPFLAGS += -DENABLE_TRACE=1
else
  CPPFLAGS += -DENABLE_TRACE=0
endif


# One Kernel module headers
#INCLUDES += -I$(KERNEL)/linux-one/include
INCLUDES += -I../lib/One
INCLUDES += -I../lib/One/linux-one/include

#
# Fusion Kernel module headers
INCLUDES += -I$(KERNEL)/linux-fusion/linux/include

#
# libvoodoo object files
LIB_VOODOO_SOURCES = \
	$(DFB_SOURCE)/lib/voodoo/client.c				\
	$(DFB_SOURCE)/lib/voodoo/conf.c				\
	$(DFB_SOURCE)/lib/voodoo/connection.cpp			\
	$(DFB_SOURCE)/lib/voodoo/connection_packet.cpp		\
	$(DFB_SOURCE)/lib/voodoo/connection_raw.cpp			\
	$(DFB_SOURCE)/lib/voodoo/connection_link.cpp			\
	$(DFB_SOURCE)/lib/voodoo/instance.cpp			\
	$(DFB_SOURCE)/lib/voodoo/init.c				\
	$(DFB_SOURCE)/lib/voodoo/interface.c			\
	$(DFB_SOURCE)/lib/voodoo/manager.cpp				\
	$(DFB_SOURCE)/lib/voodoo/manager_c.cpp			\
	$(DFB_SOURCE)/lib/voodoo/play.c				\
	$(DFB_SOURCE)/lib/voodoo/server.c				\
	$(DFB_SOURCE)/lib/voodoo/dispatcher.cpp          		\
	$(DFB_SOURCE)/lib/voodoo/unix/interfaces_unix.c		\
	$(DFB_SOURCE)/lib/voodoo/unix/link_unix.c

#
# libdirect object files
LIB_DIRECT_SOURCES = \
	$(DFB_SOURCE)/lib/direct/clock.c				\
	$(DFB_SOURCE)/lib/direct/conf.c				\
	$(DFB_SOURCE)/lib/direct/debug.c				\
	$(DFB_SOURCE)/lib/direct/direct.c				\
	$(DFB_SOURCE)/lib/direct/direct_result.c			\
	$(DFB_SOURCE)/lib/direct/fastlz.c				\
	$(DFB_SOURCE)/lib/direct/flz.c				\
	$(DFB_SOURCE)/lib/direct/hash.c				\
	$(DFB_SOURCE)/lib/direct/init.c				\
	$(DFB_SOURCE)/lib/direct/interface.c			\
	$(DFB_SOURCE)/lib/direct/list.c				\
	$(DFB_SOURCE)/lib/direct/log.c				\
	$(DFB_SOURCE)/lib/direct/log_domain.c			\
	$(DFB_SOURCE)/lib/direct/map.c				\
	$(DFB_SOURCE)/lib/direct/mem.c				\
	$(DFB_SOURCE)/lib/direct/memcpy.c				\
	$(DFB_SOURCE)/lib/direct/messages.c			\
	$(DFB_SOURCE)/lib/direct/modules.c				\
	$(DFB_SOURCE)/lib/direct/print.c				\
	$(DFB_SOURCE)/lib/direct/result.c				\
	$(DFB_SOURCE)/lib/direct/serial.c				\
	$(DFB_SOURCE)/lib/direct/signals.c				\
	$(DFB_SOURCE)/lib/direct/stream.c				\
	$(DFB_SOURCE)/lib/direct/system.c				\
	$(DFB_SOURCE)/lib/direct/thread.c				\
	$(DFB_SOURCE)/lib/direct/trace.c				\
	$(DFB_SOURCE)/lib/direct/tree.c				\
	$(DFB_SOURCE)/lib/direct/utf8.c				\
	$(DFB_SOURCE)/lib/direct/util.c				\
	$(DFB_SOURCE)/lib/direct/uuid.c				\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/clock.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/deprecated.c	\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/filesystem.c	\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/log.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/mem.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/mutex.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/signals.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/system.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/thread.c		\
	$(DFB_SOURCE)/lib/direct/os/linux/glibc/util.c

#
# libfusion object files
LIB_FUSION_SOURCES = \
	$(DFB_SOURCE)/lib/fusion/arena.c				\
	$(DFB_SOURCE)/lib/fusion/call.c				\
	$(DFB_SOURCE)/lib/fusion/conf.c				\
	$(DFB_SOURCE)/lib/fusion/fusion.c				\
	$(DFB_SOURCE)/lib/fusion/hash.c				\
	$(DFB_SOURCE)/lib/fusion/lock.c				\
	$(DFB_SOURCE)/lib/fusion/object.c				\
	$(DFB_SOURCE)/lib/fusion/property.c			\
	$(DFB_SOURCE)/lib/fusion/reactor.c				\
	$(DFB_SOURCE)/lib/fusion/ref.c				\
	$(DFB_SOURCE)/lib/fusion/shmalloc.c			\
	$(DFB_SOURCE)/lib/fusion/vector.c

LIB_FUSION_SOURCES_SINGLE = \
	$(DFB_SOURCE)/lib/fusion/shm/fake.c

LIB_FUSION_SOURCES_MULTI = \
	$(DFB_SOURCE)/lib/fusion/shm/heap.c			\
	$(DFB_SOURCE)/lib/fusion/shm/pool.c			\
	$(DFB_SOURCE)/lib/fusion/shm/shm.c

#
# New surface core object files
SURFACE_CORE_SOURCES_NEW = \
	$(DFB_SOURCE)/src/core/local_surface_pool.c		\
	$(DFB_SOURCE)/src/core/prealloc_surface_pool.c		\
	$(DFB_SOURCE)/src/core/prealloc_surface_pool_bridge.c	\
	$(DFB_SOURCE)/src/core/surface_pool_bridge.c		\
	$(DFB_SOURCE)/src/core/shared_surface_pool.c		\
	$(DFB_SOURCE)/src/core/shared_secure_surface_pool.c	\
	$(DFB_SOURCE)/src/core/surface.c				\
	$(DFB_SOURCE)/src/core/surface_buffer.c			\
	$(DFB_SOURCE)/src/core/surface_client.c			\
	$(DFB_SOURCE)/src/core/surface_core.c			\
	$(DFB_SOURCE)/src/core/surface_pool.c			\
	$(DFB_SOURCE)/src/core/surface_allocation.c

#

#
# DirectFB object files
DIRECTFB_SOURCES = \
	$(SURFACE_CORE_SOURCES_NEW)					\
	$(DFB_SOURCE)/src/core/clipboard.c				\
	$(DFB_SOURCE)/src/core/colorhash.c				\
	$(DFB_SOURCE)/src/core/core.c				\
	$(DFB_SOURCE)/src/core/core_parts.c			\
	$(DFB_SOURCE)/src/core/fonts.c				\
	$(DFB_SOURCE)/src/core/gfxcard.c				\
	$(DFB_SOURCE)/src/core/graphics_state.c			\
	$(DFB_SOURCE)/src/core/input.c				\
	$(DFB_SOURCE)/src/core/input_hub.c				\
	$(DFB_SOURCE)/src/core/layer_context.c			\
	$(DFB_SOURCE)/src/core/layer_control.c			\
	$(DFB_SOURCE)/src/core/layer_region.c			\
	$(DFB_SOURCE)/src/core/layers.c				\
	$(DFB_SOURCE)/src/core/palette.c				\
	$(DFB_SOURCE)/src/core/screen.c				\
	$(DFB_SOURCE)/src/core/screens.c				\
	$(DFB_SOURCE)/src/core/state.c				\
	$(DFB_SOURCE)/src/core/system.c				\
	$(DFB_SOURCE)/src/core/windows.c				\
	$(DFB_SOURCE)/src/core/windowstack.c			\
	$(DFB_SOURCE)/src/core/wm.c				\
	$(DFB_SOURCE)/src/directfb.c				\
	$(DFB_SOURCE)/src/directfb_result.c			\
	$(DFB_SOURCE)/src/display/idirectfbdisplaylayer.c		\
	$(DFB_SOURCE)/src/display/idirectfbpalette.c		\
	$(DFB_SOURCE)/src/display/idirectfbscreen.c		\
	$(DFB_SOURCE)/src/display/idirectfbsurface.c		\
	$(DFB_SOURCE)/src/display/idirectfbsurface_layer.c		\
	$(DFB_SOURCE)/src/display/idirectfbsurface_window.c	\
	$(DFB_SOURCE)/src/gfx/clip.c				\
	$(DFB_SOURCE)/src/gfx/convert.c				\
	$(DFB_SOURCE)/src/gfx/generic/generic.c			\
	$(DFB_SOURCE)/src/gfx/generic/generic_blit.c		\
	$(DFB_SOURCE)/src/gfx/generic/generic_draw_line.c		\
	$(DFB_SOURCE)/src/gfx/generic/generic_fill_rectangle.c	\
	$(DFB_SOURCE)/src/gfx/generic/generic_stretch_blit.c	\
	$(DFB_SOURCE)/src/gfx/generic/generic_texture_triangles.c	\
	$(DFB_SOURCE)/src/gfx/generic/generic_util.c		\
	$(DFB_SOURCE)/src/gfx/util.c				\
	$(DFB_SOURCE)/src/idirectfb.c				\
	$(DFB_SOURCE)/src/init.c					\
	$(DFB_SOURCE)/src/input/idirectfbinputbuffer.c		\
	$(DFB_SOURCE)/src/input/idirectfbinputdevice.c		\
	$(DFB_SOURCE)/src/media/DataBuffer.cpp			\
	$(DFB_SOURCE)/src/media/DataBuffer_real.cpp			\
	$(DFB_SOURCE)/src/media/idirectfbdatabuffer.c		\
	$(DFB_SOURCE)/src/media/idirectfbdatabuffer_client.c	\
	$(DFB_SOURCE)/src/media/idirectfbdatabuffer_file.c		\
	$(DFB_SOURCE)/src/media/idirectfbdatabuffer_memory.c	\
	$(DFB_SOURCE)/src/media/idirectfbdatabuffer_streamed.c	\
	$(DFB_SOURCE)/src/media/idirectfbfont.c			\
	$(DFB_SOURCE)/src/media/idirectfbimageprovider.c		\
	$(DFB_SOURCE)/src/media/idirectfbimageprovider_client.c	\
	$(DFB_SOURCE)/src/media/idirectfbvideoprovider.c		\
	$(DFB_SOURCE)/src/media/ImageProvider.cpp			\
	$(DFB_SOURCE)/src/media/ImageProvider_real.cpp		\
	$(DFB_SOURCE)/src/misc/conf.c				\
	$(DFB_SOURCE)/src/misc/gfx_util.c				\
	$(DFB_SOURCE)/src/misc/util.c				\
	$(DFB_SOURCE)/src/windows/idirectfbwindow.c		\
	$(DFB_SOURCE)/src/core/CoreDFB.cpp				\
	$(DFB_SOURCE)/src/core/CoreDFB_real.cpp			\
	$(DFB_SOURCE)/src/core/CoreGraphicsState.cpp			\
	$(DFB_SOURCE)/src/core/CoreGraphicsState_real.cpp		\
	$(DFB_SOURCE)/src/core/CoreGraphicsStateClient.c		\
	$(DFB_SOURCE)/src/core/CoreLayer.cpp				\
	$(DFB_SOURCE)/src/core/CoreLayerContext.cpp			\
	$(DFB_SOURCE)/src/core/CoreLayerContext_real.cpp		\
	$(DFB_SOURCE)/src/core/CoreLayerRegion.cpp			\
	$(DFB_SOURCE)/src/core/CoreLayerRegion_real.cpp		\
	$(DFB_SOURCE)/src/core/CoreLayer_real.cpp			\
	$(DFB_SOURCE)/src/core/CoreInputDevice.cpp			\
	$(DFB_SOURCE)/src/core/CoreInputDevice_real.cpp		\
	$(DFB_SOURCE)/src/core/CorePalette.cpp			\
	$(DFB_SOURCE)/src/core/CorePalette_real.cpp			\
	$(DFB_SOURCE)/src/core/CoreSlave.cpp				\
	$(DFB_SOURCE)/src/core/CoreSlave_real.cpp			\
	$(DFB_SOURCE)/src/core/CoreScreen.cpp			\
	$(DFB_SOURCE)/src/core/CoreScreen_real.cpp			\
	$(DFB_SOURCE)/src/core/CoreSurface.cpp			\
	$(DFB_SOURCE)/src/core/CoreSurface_real.cpp			\
	$(DFB_SOURCE)/src/core/CoreSurfaceClient.cpp		\
	$(DFB_SOURCE)/src/core/CoreSurfaceClient_real.cpp	\
	$(DFB_SOURCE)/src/core/CoreWindow.cpp			\
	$(DFB_SOURCE)/src/core/CoreWindowStack.cpp			\
	$(DFB_SOURCE)/src/core/CoreWindowStack_real.cpp		\
	$(DFB_SOURCE)/src/core/CoreWindow_real.cpp

#
# DirectFB requestor object files
DIRECTFB_SOURCES += $(DFB_SOURCE)/proxy/requestor/idirectfb_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbdatabuffer_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbdisplaylayer_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbeventbuffer_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbfont_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbimageprovider_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbinputdevice_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbpalette_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbscreen_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbsurface_requestor.c \
	$(DFB_SOURCE)/proxy/requestor/idirectfbwindow_requestor.c

#
# DirectFB dispatcher object files
DIRECTFB_SOURCES += $(DFB_SOURCE)/proxy/dispatcher/idirectfb_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbdatabuffer_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbdisplaylayer_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbeventbuffer_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbfont_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbimageprovider_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbinputdevice_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbpalette_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbscreen_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbsurface_dispatcher.c \
	$(DFB_SOURCE)/proxy/dispatcher/idirectfbwindow_dispatcher.c	

#
# DirectFB Windows extension
DIRECTFB_SOURCES += \
	$(DFB_SOURCE)/interfaces/IDirectFBWindows/idirectfbwindows_default.c	\
	$(DFB_SOURCE)/interfaces/IDirectFBWindows/idirectfbwindows_dispatcher.c	\
	$(DFB_SOURCE)/interfaces/IDirectFBWindows/idirectfbwindows_requestor.c

WM_SOURCES = \
	$(DFB_SOURCE)/wm/default/default.c

FONTPROVIDER_SOURCES = \
	$(DFB_SOURCE)/interfaces/IDirectFBFont/idirectfbfont_ft2.c

GFXDRIVER_SOURCES = \
	$(DFB_SOURCE)/gfxdrivers/gles2/gles2_2d.c	\
	$(DFB_SOURCE)/gfxdrivers/gles2/gles2_gfxdriver.c	\
	$(DFB_SOURCE)/gfxdrivers/gles2/gles2_shaders.c

#
# DirectFB System
DIRECTFB_SOURCES += \
	$(DFB_SOURCE)/systems/android/android_input.c \
	$(DFB_SOURCE)/systems/android/idirectfbimageprovider_android.c \
	$(DFB_SOURCE)/systems/android/android_layer.c \
	$(DFB_SOURCE)/systems/android/android_main.c \
	$(DFB_SOURCE)/systems/android/android_screen.c \
	$(DFB_SOURCE)/systems/android/fbo_surface_pool.c \
	$(DFB_SOURCE)/systems/android/android_system.c

# Test
DIRECTFB_SOURCES += $(CLANBOMBER_SOURCES)
#	$(DFB_SOURCE)/../DirectFB-examples/src/df_andi.c
#	$(DFB_SOURCE)/../DirectFB-examples/src/df_input.c
#	$(DFB_SOURCE)/tests/dfbtest_fillrect.c

#
# DirectFB header files
DIRECTFB_INCLUDES += \
	-I../include					\
	-I../lib					\
	-I../src					\
	-I../proxy/requestor				\
	-I../proxy/dispatcher			\
	-I../systems


#
# DiVine object files
DIVINE_SOURCES = \
	$(DFB_SOURCE)/DiVine/driver/divine.c				\
	$(DFB_SOURCE)/DiVine/lib/divine.c					\
	$(DFB_SOURCE)/DiVine/lib/idivine.c					\
	$(DFB_SOURCE)/DiVine/proxy/dispatcher/idivine_dispatcher.c		\
	$(DFB_SOURCE)/DiVine/proxy/requestor/idivine_requestor.c

#
# FusionDale header files and defines
DIVINE_INCLUDES += \
	-I../DiVine/include					\
	-I../DiVine/lib						\
	-I../DiVine/proxy/dispatcher

CPPFLAGS += -DDIVINE_MAJOR_VERSION=1 -DDIVINE_MINOR_VERSION=6

#
# SaWMan object files
SAWMAN_SOURCES = \
	$(DFB_SOURCE)/SaWMan/src/isawman.c					\
	$(DFB_SOURCE)/SaWMan/src/isawmanmanager.c				\
	$(DFB_SOURCE)/SaWMan/src/SaWMan.c					\
	$(DFB_SOURCE)/SaWMan/src/SaWMan_real.c				\
	$(DFB_SOURCE)/SaWMan/src/SaWManManager.c				\
	$(DFB_SOURCE)/SaWMan/src/SaWManManager_real.c			\
	$(DFB_SOURCE)/SaWMan/src/SaWManProcess.c				\
	$(DFB_SOURCE)/SaWMan/src/SaWManProcess_real.c			\
	$(DFB_SOURCE)/SaWMan/src/region.c					\
	$(DFB_SOURCE)/SaWMan/src/sawman_c.c					\
	$(DFB_SOURCE)/SaWMan/src/sawman_config.c				\
	$(DFB_SOURCE)/SaWMan/src/sawman_draw.c				\
	$(DFB_SOURCE)/SaWMan/src/sawman_updates.c				\
	$(DFB_SOURCE)/SaWMan/src/sawman_window.c				\
	$(DFB_SOURCE)/SaWMan/wm/sawman/sawman_wm.c

#
# SaWMan header files
SAWMAN_INCLUDES += \
	-I../SaWMan/include					\
	-I../SaWMan/src

CPPFLAGS += -DSAWMAN_VERSION=\"1.6.0\"

#
# FusionDale object files
FUSIONDALE_SOURCES = \
	$(DFB_SOURCE)/lib/One/One.c				\
	$(DFB_SOURCE)/FusionDale/one/icoma_one.c				\
	$(DFB_SOURCE)/FusionDale/one/icomacomponent_one.c			\
	$(DFB_SOURCE)/FusionDale/one/ifusiondale_one.c			\
	$(DFB_SOURCE)/FusionDale/src/ifusiondale.c				\
	$(DFB_SOURCE)/FusionDale/src/fusiondale.c				\
	$(DFB_SOURCE)/FusionDale/src/messenger/ifusiondalemessenger.c	\
	$(DFB_SOURCE)/FusionDale/src/core/dale_core.c			\
	$(DFB_SOURCE)/FusionDale/src/core/messenger.c			\
	$(DFB_SOURCE)/FusionDale/src/core/messenger_port.c			\
	$(DFB_SOURCE)/FusionDale/src/coma/coma.c				\
	$(DFB_SOURCE)/FusionDale/src/coma/component.c			\
	$(DFB_SOURCE)/FusionDale/src/coma/icoma.c				\
	$(DFB_SOURCE)/FusionDale/src/coma/icomacomponent.c			\
	$(DFB_SOURCE)/FusionDale/src/coma/policy.c        			\
	$(DFB_SOURCE)/FusionDale/src/coma/thread.c        			\
	$(DFB_SOURCE)/FusionDale/src/misc/dale_config.c			\
	$(DFB_SOURCE)/FusionDale/proxy/dispatcher/ifusiondale_dispatcher.c	\
	$(DFB_SOURCE)/FusionDale/proxy/dispatcher/icoma_dispatcher.c	\
	$(DFB_SOURCE)/FusionDale/proxy/dispatcher/icomacomponent_dispatcher.c	\
	$(DFB_SOURCE)/FusionDale/proxy/requestor/ifusiondale_requestor.c	\
	$(DFB_SOURCE)/FusionDale/proxy/requestor/icoma_requestor.c		\
	$(DFB_SOURCE)/FusionDale/proxy/requestor/icomacomponent_requestor.c

CLANBOMBER_INCLUDES = \
	-I$(DFB_SOURCE)/ClanBomber2/clanbomber \
	-I$(DFB_SOURCE)/FusionSound/include/

CLANBOMBER_SOURCES = \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_Keyboard.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Observer.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Arrow.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_None.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Bomber.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/cl_vector.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Ice.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Debug.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Joint.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_PutBomb.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Koks.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapEntry.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Corpse_Part.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_Stoned.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Thread.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Chat.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_Joystick.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/GameStatus_Team.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Menu.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_Frozen.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/ClientSetup.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/ClanBomber.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Trap.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Resources.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Viagra.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Explosion.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/GameStatus.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Box.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/ServerSetup.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Credits.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_RCMouse.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Timer.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/PlayerSetup.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_AI.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/GameObject.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Bomb.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Config.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Wall.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Map.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Glove.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Skateboard.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Bomber_Corpse.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Event.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapEditor.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Kick.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Controller_AI_mass.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Mutex.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Disease_Fast.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Bomb.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Extra_Power.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Server.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/Client.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapSelector.cpp \
	$(DFB_SOURCE)/../ClanBomber2/clanbomber/MapTile_Ground.cpp

#
#
# Build list of all objects
LOCAL_SRC_FILES := \
	$(LIB_VOODOO_SOURCES)						\
	$(LIB_DIRECT_SOURCES)						\
	$(LIB_FUSION_SOURCES)						\
	$(LIB_FUSION_SOURCES_SINGLE)					\
	$(DIRECTFB_SOURCES)						\
	$(WM_SOURCES)							\
	$(GFXDRIVER_SOURCES)						\
	$(FONTPROVIDER_SOURCES)
#	$(DIVINE_SOURCES)						\
#	$(SAWMAN_SOURCES)						\
#	$(FUSIONDALE_SOURCES)


#
# Build complete include path
INCLUDES += \
	$(DIRECTFB_INCLUDES)						\
	$(DIVINE_INCLUDES)						\
	$(SAWMAN_INCLUDES)						\
	$(FUSIONDALE_INCLUDES)						\
	$(CLANBOMBER_INCLUDES)



LOCAL_STATIC_LIBRARIES := android_native_app_glue


include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
