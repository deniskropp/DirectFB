# PSP specific stuff first
PSPSDK = $(shell psp-config --pspsdk-path)

CC       = psp-gcc
CXX      = psp-g++
AS       = psp-gcc
LD       = psp-gcc
AR       = psp-ar
RANLIB   = psp-ranlib
STRIP    = psp-strip
MKSFO    = mksfo
PACK_PBP = pack-pbp
FIXUP    = psp-fixup-imports
# Add in PSPSDK includes and libraries.
INCDIR   := $(INCDIR) . $(PSPSDK)/include
LIBDIR   := $(LIBDIR) . $(PSPSDK)/lib

CFLAGS   := $(addprefix -I,$(INCDIR)) $(CFLAGS) -DDIRECT_BUILD_OSTYPE=DIRECT_OS_PSP -Werror-implicit-function-declaration
CXXFLAGS := $(CFLAGS) $(CXXFLAGS)
ASFLAGS  := $(CFLAGS) $(ASFLAGS)

####################################################################
#
# Global setup
TOP    = .
SOURCE = $(TOP)/..

NM ?= nm

#
# Version definition
MAJOR   = 1
MINOR   = 5
TINY    = 0
VERSION = $(MAJOR).$(MINOR).$(TINY)

# Default target
default: all

#
# Compiler flags
CFLAGS += -fpic -pipe -O2 -Wall

#
# Linker flags
LDFLAGS += -L.

#
# Configuration header files
INCLUDES += -I$(TOP)

#
# Preprocessor flags
CPPFLAGS += \
	-DHAVE_CONFIG_H							\
	-DHAVE_STDBOOL_H						\
	-D_GNU_SOURCE							\
	-D_REENTRANT							\
	-DVERSION=\"$(VERSION)\"					\
        -DBUILDTIME="\"$(buildtime)\""

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

####################################################################################

DESTDIR = /usr/local/pspdev/
prefix  = /usr/local/pspdev

datadir = /usr/local/share/directfb-examples

# Compiler flags
CFLAGS += -ffast-math -Wno-strict-aliasing -fPIC -Wno-error

#
# Take system freetype
INCLUDES += $(shell pkg-config --cflags freetype2)
LIBS     += $(shell pkg-config --libs freetype2)

#
# Take system jpeg
LIBS     += -ljpeg

#
# Take system png
LIBS     += -lpng -lz

##################################################################################
#
# Installation paths
prefix     ?= /opt/directfb
bindir     ?= $(prefix)/bin
datadir    ?= $(prefix)/share
libdir     ?= $(prefix)/lib
moddir     ?= $(libdir)/directfb
sysconfdir ?= $(prefix)/etc

CPPFLAGS += \
	-DDATADIR=\"$(datadir)\"	\
	-DMODULEDIR=\"$(moddir)\"	\
	-DSYSCONFDIR=\"$(sysconfdir)\"

buildtime = $(shell sh -c date -u +%Y-%m-%d %H:%M)

#
# libdirect object files
LIB_DIRECT_OBJECTS = \
	$(SOURCE)/lib/direct/clock.o				\
	$(SOURCE)/lib/direct/conf.o				\
	$(SOURCE)/lib/direct/debug.o				\
	$(SOURCE)/lib/direct/direct.o				\
	$(SOURCE)/lib/direct/direct_result.o			\
	$(SOURCE)/lib/direct/fifo.o				\
	$(SOURCE)/lib/direct/hash.o				\
	$(SOURCE)/lib/direct/interface.o			\
	$(SOURCE)/lib/direct/log.o				\
	$(SOURCE)/lib/direct/log_domain.o			\
	$(SOURCE)/lib/direct/list.o				\
	$(SOURCE)/lib/direct/map.o				\
	$(SOURCE)/lib/direct/mem.o				\
	$(SOURCE)/lib/direct/memcpy.o				\
	$(SOURCE)/lib/direct/messages.o				\
	$(SOURCE)/lib/direct/modules.o				\
	$(SOURCE)/lib/direct/print.o				\
	$(SOURCE)/lib/direct/processor.o			\
	$(SOURCE)/lib/direct/result.o				\
	$(SOURCE)/lib/direct/serial.o				\
	$(SOURCE)/lib/direct/signals.o				\
	$(SOURCE)/lib/direct/stream.o				\
	$(SOURCE)/lib/direct/system.o				\
	$(SOURCE)/lib/direct/thread.o				\
	$(SOURCE)/lib/direct/trace.o				\
	$(SOURCE)/lib/direct/tree.o				\
	$(SOURCE)/lib/direct/utf8.o				\
	$(SOURCE)/lib/direct/util.o				\
	$(SOURCE)/lib/direct/os/psp/clock.o			\
	$(SOURCE)/lib/direct/os/psp/deprecated.o		\
	$(SOURCE)/lib/direct/os/psp/filesystem.o		\
	$(SOURCE)/lib/direct/os/psp/log.o			\
	$(SOURCE)/lib/direct/os/psp/mem.o			\
	$(SOURCE)/lib/direct/os/psp/signals.o			\
	$(SOURCE)/lib/direct/os/psp/system.o			\
	$(SOURCE)/lib/direct/os/psp/thread.o


#
# libfusion object files
LIB_FUSION_OBJECTS = \
	$(SOURCE)/lib/fusion/arena.o				\
	$(SOURCE)/lib/fusion/call.o				\
	$(SOURCE)/lib/fusion/conf.o				\
	$(SOURCE)/lib/fusion/fusion.o				\
	$(SOURCE)/lib/fusion/hash.o				\
	$(SOURCE)/lib/fusion/lock.o				\
	$(SOURCE)/lib/fusion/object.o				\
	$(SOURCE)/lib/fusion/property.o			\
	$(SOURCE)/lib/fusion/reactor.o				\
	$(SOURCE)/lib/fusion/ref.o				\
	$(SOURCE)/lib/fusion/shmalloc.o			\
	$(SOURCE)/lib/fusion/vector.o

LIB_FUSION_OBJECTS_SINGLE = \
	$(SOURCE)/lib/fusion/shm/fake.o

#
# New surface core object files
SURFACE_CORE_OBJECTS_NEW = \
	$(SOURCE)/src/core/local_surface_pool.o		\
	$(SOURCE)/src/core/prealloc_surface_pool.o		\
	$(SOURCE)/src/core/shared_surface_pool.o		\
	$(SOURCE)/src/core/surface.o				\
	$(SOURCE)/src/core/surface_buffer.o			\
	$(SOURCE)/src/core/surface_core.o			\
	$(SOURCE)/src/core/surface_pool.o			\
	$(SOURCE)/src/core/surface_pool_bridge.o

#

#
# DirectFB object files
DIRECTFB_OBJECTS = \
	$(SURFACE_CORE_OBJECTS_NEW)					\
	$(SOURCE)/src/core/clipboard.o				\
	$(SOURCE)/src/core/colorhash.o				\
	$(SOURCE)/src/core/core.o				\
	$(SOURCE)/src/core/core_parts.o			\
	$(SOURCE)/src/core/fonts.o				\
	$(SOURCE)/src/core/gfxcard.o				\
	$(SOURCE)/src/core/input.o				\
	$(SOURCE)/src/core/layer_context.o			\
	$(SOURCE)/src/core/layer_control.o			\
	$(SOURCE)/src/core/layer_region.o			\
	$(SOURCE)/src/core/layers.o				\
	$(SOURCE)/src/core/palette.o				\
	$(SOURCE)/src/core/screen.o				\
	$(SOURCE)/src/core/screens.o				\
	$(SOURCE)/src/core/state.o				\
	$(SOURCE)/src/core/system.o				\
	$(SOURCE)/src/core/windows.o				\
	$(SOURCE)/src/core/windowstack.o			\
	$(SOURCE)/src/core/wm.o				\
	$(SOURCE)/src/directfb.o				\
	$(SOURCE)/src/display/idirectfbdisplaylayer.o		\
	$(SOURCE)/src/display/idirectfbpalette.o		\
	$(SOURCE)/src/display/idirectfbscreen.o		\
	$(SOURCE)/src/display/idirectfbsurface.o		\
	$(SOURCE)/src/display/idirectfbsurface_layer.o		\
	$(SOURCE)/src/display/idirectfbsurface_window.o	\
	$(SOURCE)/src/gfx/clip.o				\
	$(SOURCE)/src/gfx/convert.o				\
	$(SOURCE)/src/gfx/generic/generic.o			\
	$(SOURCE)/src/gfx/util.o				\
	$(SOURCE)/src/idirectfb.o				\
	$(SOURCE)/src/input/idirectfbinputbuffer.o		\
	$(SOURCE)/src/input/idirectfbinputdevice.o		\
	$(SOURCE)/src/media/idirectfbdatabuffer.o		\
	$(SOURCE)/src/media/idirectfbdatabuffer_file.o		\
	$(SOURCE)/src/media/idirectfbdatabuffer_memory.o	\
	$(SOURCE)/src/media/idirectfbdatabuffer_streamed.o	\
	$(SOURCE)/src/media/idirectfbfont.o			\
	$(SOURCE)/src/media/idirectfbimageprovider.o		\
	$(SOURCE)/src/media/idirectfbvideoprovider.o		\
	$(SOURCE)/src/misc/conf.o				\
	$(SOURCE)/src/misc/gfx_util.o				\
	$(SOURCE)/src/misc/util.o				\
	$(SOURCE)/src/windows/idirectfbwindow.o

#
# DirectFB requestor object files
DIRECTFB_OBJECTS += $(SOURCE)/proxy/requestor/idirectfb_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbdatabuffer_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbdisplaylayer_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbeventbuffer_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbfont_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbimageprovider_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbinputdevice_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbpalette_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbscreen_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbsurface_requestor.o \
	$(SOURCE)/proxy/requestor/idirectfbwindow_requestor.o

#
# DirectFB dispatcher object files
DIRECTFB_OBJECTS += $(SOURCE)/proxy/dispatcher/idirectfb_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbdatabuffer_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbdisplaylayer_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbeventbuffer_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbfont_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbimageprovider_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbinputdevice_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbpalette_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbscreen_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbsurface_dispatcher.o \
	$(SOURCE)/proxy/dispatcher/idirectfbwindow_dispatcher.o	
#
# DirectFB header files
DIRECTFB_INCLUDES += \
	-I$(SOURCE)/include					\
	-I$(SOURCE)/lib					\
	-I$(SOURCE)/src					\
	-I$(SOURCE)/proxy/requestor					\
	-I$(SOURCE)/proxy/dispatcher					\
	-I$(SOURCE)/systems


#
# Build list of all objects
OBJECTS += \
	$(LIB_DIRECT_OBJECTS)						\
	$(LIB_FUSION_OBJECTS)						\
	$(DIRECTFB_OBJECTS)						\
	$(DIVINE_OBJECTS)						\
	$(SAWMAN_OBJECTS)						\
	$(FUSIONDALE_OBJECTS)

#
# Build complete include path
INCLUDES += \
	$(DIRECTFB_INCLUDES)						\
	$(DIVINE_INCLUDES)						\
	$(SAWMAN_INCLUDES)						\
	$(FUSIONDALE_INCLUDES)



#
# All libraries
LIBS += \
	-lfreetype							\
	-lm								\
	-lpthread							\
	-ldl


#
# All tools and tests
TOOLS += \
	$(SOURCE)/tools/dfbinfo				\
	$(SOURCE)/tools/dfbdump				\
	$(SOURCE)/tools/dfbproxy				\
	$(SOURCE)/tools/fusion_bench				\
	$(SOURCE)/tests/fusion_reactor				\
	$(SOURCE)/SaWMan/tools/swmdump

#
# List of generated header files
GEN_HEADERS = \
	$(SOURCE)/include/directfb_keynames.h			\
	$(SOURCE)/include/directfb_strings.h			\
	$(SOURCE)/include/directfb_version.h


#
# Rules
all: $(GEN_HEADERS) libdirectfb.o libdirectfb.so libdirectfb.so.nm $(TOOLS) $(TEST) $(TOOLS:%=%.nm)

libdirectfb.o:  $(GEN_HEADERS) $(OBJECTS)
	$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS)

libdirectfb.so: $(GEN_HEADERS) $(OBJECTS)
	$(CC) -shared -Wl,-soname,libdirectfb.so $(LDFLAGS) $(LIBS) -o $@ $(OBJECTS)

libdirectfb.so.nm: libdirectfb.so
	$(NM) -n libdirectfb.so > libdirectfb.so.nm

%.o: %.c
	$(CC) $(CFLAGS) -c $(CPPFLAGS) $(INCLUDES) -o $@ $<

$(SOURCE)/tests/%: $(SOURCE)/tests/%.o libdirectfb.so
	$(CC) $(LDFLAGS) -o $@ $< -ldirectfb $(LIBS)

$(SOURCE)/tools/%: $(SOURCE)/tools/%.o libdirectfb.so
	$(CC) $(LDFLAGS) -o $@ $< -ldirectfb $(LIBS)

$(SOURCE)/SaWMan/tools/%: $(SOURCE)/SaWMan/tools/%.o libdirectfb.so
	$(CC) $(LDFLAGS) -o $@ $< -ldirectfb $(LIBS)

%.nm: %
	$(NM) -n $< > $@


.PHONY: clean install

clean::
	rm -f libdirectfb.*o* $(TOOLS) $(TEST) $(GEN_HEADERS)
	find $(TOP) -name "*.o" | xargs rm -f
	find $(TOP) -name "*.so" | xargs rm -f
	find $(TOP) -name "*.nm" | xargs rm -f

install:: all
	@(if test -z "$(DESTDIR)"; then echo "**** DESTDIR not set! ****"; exit 1; fi)
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(libdir)
	install -d $(DESTDIR)$(moddir)
	install -d $(DESTDIR)$(moddir)/inputdrivers
	install -d $(DESTDIR)$(moddir)/interfaces
	install -d $(DESTDIR)$(moddir)/gfxdrivers
	install -d $(DESTDIR)$(moddir)/systems
	install -d $(DESTDIR)$(moddir)/wm
	install -c -m 755 $(TEST) $(TOOLS) $(TOOLS:%=%.nm) $(DESTDIR)$(bindir)/
	install -c -m 755 libdirectfb.so $(DESTDIR)$(libdir)/
	install -c -m 644 libdirectfb.so.nm $(DESTDIR)$(libdir)/




DFBINC  = $(SOURCE)/include
MKNAMES = $(SOURCE)/tools/mknames.sh


$(DFBINC)/directfb_version.h: Makefile
	@echo '#ifndef __DIRECTFB_VERSION_H__' > $@
	@echo '#define __DIRECTFB_VERSION_H__' >> $@
	@echo '' >> $@
	@echo '#define DIRECTFB_MAJOR_VERSION  (' $(MAJOR) ')' >> $@
	@echo '#define DIRECTFB_MINOR_VERSION  (' $(MINOR) ')' >> $@
	@echo '#define DIRECTFB_MICRO_VERSION  (' $(TINY) ')' >> $@
	@echo '#define DIRECTFB_BINARY_AGE     (0)' >> $@
	@echo '#define DIRECTFB_INTERFACE_AGE  (0)' >> $@
	@echo '' >> $@
	@echo '' >> $@
	@echo '#endif' >> $@


$(DFBINC)/directfb_keynames.h: $(DFBINC)/directfb_keyboard.h $(MKNAMES) Makefile
	@echo '#ifndef __DIRECTFB_KEYNAMES_H__' > $@
	@echo '#define __DIRECTFB_KEYNAMES_H__' >> $@

	$(MKNAMES) DFBInputDeviceKeySymbol DIKS NULL KeySymbol symbol $(DFBINC)/directfb_keyboard.h \
	   | grep -v DIKS_ENTER >> $@

	$(MKNAMES) DFBInputDeviceKeyIdentifier DIKI UNKNOWN KeyIdentifier identifier $(DFBINC)/directfb_keyboard.h \
	   | grep -v DIKI_NUMBER_OF_KEYS | grep -v DIKI_KEYDEF_END >> $@

	@echo '' >> $@
	@echo '#endif' >> $@


$(DFBINC)/directfb_strings.h: $(DFBINC)/directfb.h $(MKNAMES) Makefile
	@echo '#ifndef __DIRECTFB_STRINGS_H__' > $@
	@echo '#define __DIRECTFB_STRINGS_H__' >> $@

	$(MKNAMES) DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format $(DFBINC)/directfb.h >> $@

	$(MKNAMES) DFBInputDeviceTypeFlags DIDTF NONE InputDeviceTypeFlags type $(DFBINC)/directfb.h \
	   | grep -v DIDTF_ALL >> $@

	$(MKNAMES) DFBSurfaceDrawingFlags DSDRAW NOFX SurfaceDrawingFlags flag $(DFBINC)/directfb.h \
	   | grep -v DSDRAW_ALL >> $@

	$(MKNAMES) DFBSurfaceBlittingFlags DSBLIT NOFX SurfaceBlittingFlags flag $(DFBINC)/directfb.h \
	   | grep -v DSBLIT_ALL >> $@

	@echo FIXME: Use DIMCAPS prefix for DFBImageCapabilities
	$(MKNAMES) DFBInputDeviceCapabilities DICAPS NONE InputDeviceCapabilities capability $(DFBINC)/directfb.h \
	   | grep -v DICAPS_ALL | grep -v DICAPS_ALPHACHANNEL | grep -v DICAPS_COLORKEY >> $@

	$(MKNAMES) DFBDisplayLayerTypeFlags DLTF NONE DisplayLayerTypeFlags type $(DFBINC)/directfb.h \
	   | grep -v DLTF_ALL >> $@

	$(MKNAMES) DFBDisplayLayerCapabilities DLCAPS NONE DisplayLayerCapabilities capability $(DFBINC)/directfb.h \
	   | grep -v DLCAPS_ALL >> $@

	$(MKNAMES) DFBDisplayLayerBufferMode DLBM UNKNOWN DisplayLayerBufferMode mode $(DFBINC)/directfb.h \
	   | grep -v DLBM_DONTCARE | grep -v DLBM_COLOR | grep -v DLBM_IMAGE | grep -v DLBM_TILE >> $@

	$(MKNAMES) DFBScreenCapabilities DSCCAPS NONE ScreenCapabilities capability $(DFBINC)/directfb.h \
	   | grep -v DSCCAPS_ALL >> $@

	$(MKNAMES) DFBScreenEncoderCapabilities DSECAPS NONE ScreenEncoderCapabilities capability $(DFBINC)/directfb.h \
	   | grep -v DSECAPS_ALL >> $@

	$(MKNAMES) DFBScreenEncoderType DSET UNKNOWN ScreenEncoderType type $(DFBINC)/directfb.h \
	   | grep -v DSET_ALL >> $@

	$(MKNAMES) DFBScreenEncoderTVStandards DSETV UNKNOWN ScreenEncoderTVStandards standard $(DFBINC)/directfb.h \
	   | grep -v DSETV_ALL >> $@

	$(MKNAMES) DFBScreenOutputCapabilities DSOCAPS NONE ScreenOutputCapabilities capability $(DFBINC)/directfb.h \
	   | grep -v DSOCAPS_ALL >> $@

	$(MKNAMES) DFBScreenOutputConnectors DSOC UNKNOWN ScreenOutputConnectors connector $(DFBINC)/directfb.h \
	   | grep -v DSOC_ALL >> $@

	$(MKNAMES) DFBScreenOutputSignals DSOS NONE ScreenOutputSignals signal $(DFBINC)/directfb.h \
	   | grep -v DSOS_ALL >> $@

	$(MKNAMES) DFBScreenOutputSlowBlankingSignals DSOSB OFF ScreenOutputSlowBlankingSignals slow_signal $(DFBINC)/directfb.h \
	   | grep -v DSOSB_ALL >> $@

	$(MKNAMES) DFBScreenOutputResolution DSOR UNKNOWN ScreenOutputResolution resolution $(DFBINC)/directfb.h \
	   | grep -v DSOR_ALL >> $@

	$(MKNAMES) DFBScreenMixerCapabilities DSMCAPS NONE ScreenMixerCapabilities capability $(DFBINC)/directfb.h \
	   | grep -v DSMCAPS_ALL >> $@

	$(MKNAMES) DFBScreenMixerTree DSMT UNKNOWN ScreenMixerTree tree $(DFBINC)/directfb.h \
	   | grep -v DSMT_ALL >> $@

	$(MKNAMES) DFBScreenEncoderTestPicture DSETP OFF ScreenEncoderTestPicture test_picture $(DFBINC)/directfb.h \
	   | grep -v DSETP_ALL >> $@

	$(MKNAMES) DFBScreenEncoderScanMode DSESM UNKNOWN ScreenEncoderScanMode scan_mode $(DFBINC)/directfb.h \
	   | grep -v DSESM_ALL >> $@

	$(MKNAMES) DFBAccelerationMask DFXL NONE AccelerationMask mask $(DFBINC)/directfb.h \
	   | grep -v DFXL_ALL >> $@

	@echo '' >> $@
	@echo '#endif' >> $@

