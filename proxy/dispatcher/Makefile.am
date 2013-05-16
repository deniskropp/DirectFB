## Makefile.am for DirectFB/proxy/dispatcher

INTERFACES_DIR = $(MODULEDIR)/interfaces

idirectfbdir              = $(INTERFACES_DIR)/IDirectFB
idirectfbdatabufferdir    = $(INTERFACES_DIR)/IDirectFBDataBuffer
idirectfbdisplaylayerdir  = $(INTERFACES_DIR)/IDirectFBDisplayLayer
idirectfbeventbufferdir   = $(INTERFACES_DIR)/IDirectFBEventBuffer
idirectfbfontdir          = $(INTERFACES_DIR)/IDirectFBFont
idirectfbimageproviderdir = $(INTERFACES_DIR)/IDirectFBImageProvider
idirectfbinputdevicedir   = $(INTERFACES_DIR)/IDirectFBInputDevice
idirectfbpalettedir       = $(INTERFACES_DIR)/IDirectFBPalette
idirectfbscreendir        = $(INTERFACES_DIR)/IDirectFBScreen
idirectfbsurfacedir       = $(INTERFACES_DIR)/IDirectFBSurface
idirectfbwindowdir        = $(INTERFACES_DIR)/IDirectFBWindow
idirectfbvideoproviderdir = $(INTERFACES_DIR)/IDirectFBVideoProvider

ifusiondaledir            = $(INTERFACES_DIR)/IFusionDale
icomadir                  = $(INTERFACES_DIR)/IComa
icomacomponentdir         = $(INTERFACES_DIR)/IComaComponent

ifusionsounddir           = $(INTERFACES_DIR)/IFusionSound
ifusionsoundstreamdir     = $(INTERFACES_DIR)/IFusionSoundStream
ifusionsoundbufferdir     = $(INTERFACES_DIR)/IFusionSoundBuffer
ifusionsoundplaybackdir   = $(INTERFACES_DIR)/IFusionSoundPlayback

idivinedir = $(INTERFACES_DIR)/IDiVine


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib/divine	\
	-I$(top_builddir)/lib/fusiondale \
	-I$(top_builddir)/lib/fusionsound \
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/lib/divine	\
	-I$(top_srcdir)/lib/fusiondale	\
	-I$(top_srcdir)/lib/fusionsound	\
	-I$(top_srcdir)/src 		\
	-I$(top_srcdir)/proxy/requestor	\
	-I$(top_srcdir)/proxy/compressor	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

LIBS = \
	$(top_builddir)/lib/voodoo/libvoodoo.la \
	$(top_builddir)/lib/direct/libdirect.la


idirectfb_LTLIBRARIES = \
	libidirectfb_dispatcher.la

idirectfbdatabuffer_LTLIBRARIES = \
	libidirectfbdatabuffer_dispatcher.la

idirectfbdisplaylayer_LTLIBRARIES = \
	libidirectfbdisplaylayer_dispatcher.la

idirectfbeventbuffer_LTLIBRARIES = \
	libidirectfbeventbuffer_dispatcher.la

idirectfbfont_LTLIBRARIES = \
	libidirectfbfont_dispatcher.la

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_dispatcher.la

idirectfbinputdevice_LTLIBRARIES = \
	libidirectfbinputdevice_dispatcher.la

idirectfbpalette_LTLIBRARIES = \
	libidirectfbpalette_dispatcher.la

idirectfbscreen_LTLIBRARIES = \
	libidirectfbscreen_dispatcher.la

idirectfbsurface_LTLIBRARIES = \
	libidirectfbsurface_dispatcher.la

idirectfbwindow_LTLIBRARIES = \
	libidirectfbwindow_dispatcher.la

if ENABLE_FUSIONDALE
ifusiondale_LTLIBRARIES = \
	libifusiondale_dispatcher.la

icoma_LTLIBRARIES = \
	libicoma_dispatcher.la

icomacomponent_LTLIBRARIES = \
	libicomacomponent_dispatcher.la
endif

if ENABLE_FUSIONSOUND
ifusionsound_LTLIBRARIES = \
	libifusionsound_dispatcher.la

ifusionsoundstream_LTLIBRARIES = \
	libifusionsoundstream_dispatcher.la

ifusionsoundbuffer_LTLIBRARIES = \
	libifusionsoundbuffer_dispatcher.la

ifusionsoundplayback_LTLIBRARIES = \
	libifusionsoundplayback_dispatcher.la
endif

if ENABLE_DIVINE
idivine_LTLIBRARIES = \
	libidivine_dispatcher.la
endif

if BUILD_STATIC
idirectfb_DATA              = libidirectfb_dispatcher.o
idirectfbdatabuffer_DATA    = libidirectfbdatabuffer_dispatcher.o
idirectfbdisplaylayer_DATA  = libidirectfbdisplaylayer_dispatcher.o
idirectfbeventbuffer_DATA   = libidirectfbeventbuffer_dispatcher.o
idirectfbfont_DATA          = libidirectfbfont_dispatcher.o
idirectfbimageprovider_DATA = libidirectfbimageprovider_dispatcher.o
idirectfbinputdevice_DATA   = libidirectfbinputdevice_dispatcher.o
idirectfbpalette_DATA       = libidirectfbpalette_dispatcher.o
idirectfbscreen_DATA        = libidirectfbscreen_dispatcher.o
idirectfbsurface_DATA       = libidirectfbsurface_dispatcher.o
idirectfbwindow_DATA        = libidirectfbwindow_dispatcher.o
if ENABLE_FUSIONDALE
ifusiondale_DATA    = libifusiondale_dispatcher.o
icoma_DATA          = libicoma_dispatcher.o
icomacomponent_DATA = libicomacomponent_dispatcher.o
endif
if ENABLE_FUSIONSOUND
ifusionsound_DATA           = libifusionsound_dispatcher.o
ifusionsoundbuffer_DATA     = libifusionsoundbuffer_dispatcher.o
ifusionsoundplayback_DATA   = libifusionsoundplayback_dispatcher.o
ifusionsoundstream_DATA     = libifusionsoundstream_dispatcher.o
endif
if ENABLE_DIVINE
idivine_DATA                = libidivine_dispatcher.o
endif
endif

libidirectfb_dispatcher_la_SOURCES = idirectfb_dispatcher.c idirectfb_dispatcher.h
libidirectfb_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfb_dispatcher_la_LIBADD = $(LIBS)

libidirectfbdatabuffer_dispatcher_la_SOURCES = idirectfbdatabuffer_dispatcher.c idirectfbdatabuffer_dispatcher.h
libidirectfbdatabuffer_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbdatabuffer_dispatcher_la_LIBADD = $(LIBS)

libidirectfbdisplaylayer_dispatcher_la_SOURCES = idirectfbdisplaylayer_dispatcher.c idirectfbdisplaylayer_dispatcher.h
libidirectfbdisplaylayer_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbdisplaylayer_dispatcher_la_LIBADD = $(LIBS)

libidirectfbeventbuffer_dispatcher_la_SOURCES = idirectfbeventbuffer_dispatcher.c idirectfbeventbuffer_dispatcher.h
libidirectfbeventbuffer_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbeventbuffer_dispatcher_la_LIBADD = $(LIBS)

libidirectfbfont_dispatcher_la_SOURCES = idirectfbfont_dispatcher.c idirectfbfont_dispatcher.h
libidirectfbfont_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbfont_dispatcher_la_LIBADD = $(LIBS)

libidirectfbimageprovider_dispatcher_la_SOURCES = idirectfbimageprovider_dispatcher.c idirectfbimageprovider_dispatcher.h
libidirectfbimageprovider_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbimageprovider_dispatcher_la_LIBADD = $(LIBS)

libidirectfbinputdevice_dispatcher_la_SOURCES = idirectfbinputdevice_dispatcher.c idirectfbinputdevice_dispatcher.h
libidirectfbinputdevice_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbinputdevice_dispatcher_la_LIBADD = $(LIBS)

libidirectfbpalette_dispatcher_la_SOURCES = idirectfbpalette_dispatcher.c idirectfbpalette_dispatcher.h
libidirectfbpalette_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbpalette_dispatcher_la_LIBADD = $(LIBS)

libidirectfbscreen_dispatcher_la_SOURCES = idirectfbscreen_dispatcher.c idirectfbscreen_dispatcher.h
libidirectfbscreen_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbscreen_dispatcher_la_LIBADD = $(LIBS)

libidirectfbsurface_dispatcher_la_SOURCES = idirectfbsurface_dispatcher.c idirectfbsurface_dispatcher.h
libidirectfbsurface_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbsurface_dispatcher_la_LIBADD = $(LIBS)

libidirectfbwindow_dispatcher_la_SOURCES = idirectfbwindow_dispatcher.c idirectfbwindow_dispatcher.h
libidirectfbwindow_dispatcher_la_LDFLAGS = -avoid-version -module
libidirectfbwindow_dispatcher_la_LIBADD = $(LIBS)

libifusiondale_dispatcher_la_SOURCES = ifusiondale_dispatcher.c ifusiondale_dispatcher.h
libifusiondale_dispatcher_la_LDFLAGS = -avoid-version -module
libifusiondale_dispatcher_la_LIBADD  = $(LIBS)

libicoma_dispatcher_la_SOURCES = icoma_dispatcher.c icoma_dispatcher.h
libicoma_dispatcher_la_LDFLAGS = -avoid-version -module
libicoma_dispatcher_la_LIBADD  = $(LIBS)

libicomacomponent_dispatcher_la_SOURCES = icomacomponent_dispatcher.c icomacomponent_dispatcher.h
libicomacomponent_dispatcher_la_LDFLAGS = -avoid-version -module
libicomacomponent_dispatcher_la_LIBADD  = $(LIBS)

libifusionsound_dispatcher_la_SOURCES = ifusionsound_dispatcher.c ifusionsound_dispatcher.h
libifusionsound_dispatcher_la_LDFLAGS = -avoid-version -module
libifusionsound_dispatcher_la_LIBADD = $(LIBS)

libifusionsoundbuffer_dispatcher_la_SOURCES = ifusionsoundbuffer_dispatcher.c ifusionsoundbuffer_dispatcher.h
libifusionsoundbuffer_dispatcher_la_LDFLAGS = -avoid-version -module
libifusionsoundbuffer_dispatcher_la_LIBADD = $(LIBS) ../compressor/libfusionsound_compressor.la

libifusionsoundplayback_dispatcher_la_SOURCES = ifusionsoundplayback_dispatcher.c ifusionsoundplayback_dispatcher.h
libifusionsoundplayback_dispatcher_la_LDFLAGS = -avoid-version -module
libifusionsoundplayback_dispatcher_la_LIBADD = $(LIBS)

libifusionsoundstream_dispatcher_la_SOURCES = ifusionsoundstream_dispatcher.c ifusionsoundstream_dispatcher.h
libifusionsoundstream_dispatcher_la_LDFLAGS = -avoid-version -module
libifusionsoundstream_dispatcher_la_LIBADD = $(LIBS) ../compressor/libfusionsound_compressor.la

libidivine_dispatcher_la_SOURCES = idivine_dispatcher.c idivine_dispatcher.h
libidivine_dispatcher_la_LDFLAGS = -avoid-version -module
libidivine_dispatcher_la_LIBADD = $(LIBS) $(top_builddir)/lib/divine/libdivine.la

include $(top_srcdir)/rules/libobject.make

