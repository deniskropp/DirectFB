## Makefile.am for DirectFB/proxy/requestor

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

idivinedir                = $(INTERFACES_DIR)/IDiVine

INCLUDES = \
	-I$(top_builddir)/include		\
	-I$(top_builddir)/lib			\
	-I$(top_builddir)/lib/divine		\
	-I$(top_builddir)/lib/fusiondale	\
	-I$(top_builddir)/lib/fusionsound	\
	-I$(top_srcdir)/include			\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/lib/divine		\
	-I$(top_srcdir)/lib/fusiondale		\
	-I$(top_srcdir)/lib/fusionsound		\
	-I$(top_srcdir)/src 			\
	-I$(top_srcdir)/proxy/dispatcher	\
	-I$(top_srcdir)/proxy/compressor	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

LIBS = \
	$(top_builddir)/lib/voodoo/libvoodoo.la	\
	$(top_builddir)/lib/direct/libdirect.la


idirectfb_LTLIBRARIES = \
	libidirectfb_requestor.la

idirectfbdatabuffer_LTLIBRARIES = \
	libidirectfbdatabuffer_requestor.la

idirectfbdisplaylayer_LTLIBRARIES = \
	libidirectfbdisplaylayer_requestor.la

idirectfbeventbuffer_LTLIBRARIES = \
	libidirectfbeventbuffer_requestor.la

idirectfbfont_LTLIBRARIES = \
	libidirectfbfont_requestor.la

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_requestor.la

idirectfbinputdevice_LTLIBRARIES = \
	libidirectfbinputdevice_requestor.la

idirectfbpalette_LTLIBRARIES = \
	libidirectfbpalette_requestor.la

idirectfbscreen_LTLIBRARIES = \
	libidirectfbscreen_requestor.la

idirectfbsurface_LTLIBRARIES = \
	libidirectfbsurface_requestor.la

idirectfbwindow_LTLIBRARIES = \
	libidirectfbwindow_requestor.la

if ENABLE_FUSIONDALE
ifusiondale_LTLIBRARIES = \
	libifusiondale_requestor.la

icoma_LTLIBRARIES = \
	libicoma_requestor.la

icomacomponent_LTLIBRARIES = \
	libicomacomponent_requestor.la
endif

if ENABLE_FUSIONSOUND
ifusionsound_LTLIBRARIES = \
	libifusionsound_requestor.la

ifusionsoundstream_LTLIBRARIES = \
	libifusionsoundstream_requestor.la

ifusionsoundbuffer_LTLIBRARIES = \
	libifusionsoundbuffer_requestor.la

ifusionsoundplayback_LTLIBRARIES = \
	libifusionsoundplayback_requestor.la
endif

if ENABLE_DIVINE
idivine_LTLIBRARIES = \
	libidivine_requestor.la
endif

if BUILD_STATIC
idirectfb_DATA              = libidirectfb_requestor.o
idirectfbdatabuffer_DATA    = libidirectfbdatabuffer_requestor.o
idirectfbdisplaylayer_DATA  = libidirectfbdisplaylayer_requestor.o
idirectfbeventbuffer_DATA   = libidirectfbeventbuffer_requestor.o
idirectfbfont_DATA          = libidirectfbfont_requestor.o
idirectfbimageprovider_DATA = libidirectfbimageprovider_requestor.o
idirectfbinputdevice_DATA   = libidirectfbinputdevice_requestor.o
idirectfbpalette_DATA       = libidirectfbpalette_requestor.o
idirectfbscreen_DATA        = libidirectfbscreen_requestor.o
idirectfbsurface_DATA       = libidirectfbsurface_requestor.o
idirectfbwindow_DATA        = libidirectfbwindow_requestor.o
if ENABLE_FUSIONDALE
ifusiondale_DATA            = libifusiondale_requestor.o
icoma_DATA                  = libicoma_requestor.o
icomacomponent_DATA         = libicomacomponent_requestor.o
endif
if ENABLE_FUSIONSOUND
ifusionsound_DATA           = libifusionsound_requestor.o
ifusionsoundstream_DATA     = libifusionsoundstream_requestor.o
ifusionsoundbuffer_DATA     = libifusionsoundbuffer_requestor.o
ifusionsoundplayback_DATA   = libifusionsoundplayback_requestor.o
endif
if ENABLE_DIVINE
idivine_DATA              = libidivine_requestor.o
endif
endif


libidirectfb_requestor_la_SOURCES = idirectfb_requestor.c
libidirectfb_requestor_la_LDFLAGS = -avoid-version -module
libidirectfb_requestor_la_LIBADD  = $(LIBS)

libidirectfbdatabuffer_requestor_la_SOURCES = idirectfbdatabuffer_requestor.c
libidirectfbdatabuffer_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbdatabuffer_requestor_la_LIBADD  = $(LIBS)

libidirectfbdisplaylayer_requestor_la_SOURCES = idirectfbdisplaylayer_requestor.c idirectfbdisplaylayer_requestor.h
libidirectfbdisplaylayer_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbdisplaylayer_requestor_la_LIBADD  = $(LIBS)

libidirectfbeventbuffer_requestor_la_SOURCES = idirectfbeventbuffer_requestor.c idirectfbeventbuffer_requestor.h
libidirectfbeventbuffer_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbeventbuffer_requestor_la_LIBADD  = $(LIBS)

libidirectfbfont_requestor_la_SOURCES = idirectfbfont_requestor.c idirectfbfont_requestor.h
libidirectfbfont_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbfont_requestor_la_LIBADD  = $(LIBS)

libidirectfbimageprovider_requestor_la_SOURCES = idirectfbimageprovider_requestor.c
libidirectfbimageprovider_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbimageprovider_requestor_la_LIBADD  = $(LIBS)

libidirectfbinputdevice_requestor_la_SOURCES = idirectfbinputdevice_requestor.c idirectfbinputdevice_requestor.h
libidirectfbinputdevice_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbinputdevice_requestor_la_LIBADD  = $(LIBS)

libidirectfbpalette_requestor_la_SOURCES = idirectfbpalette_requestor.c idirectfbpalette_requestor.h
libidirectfbpalette_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbpalette_requestor_la_LIBADD  = $(LIBS)

libidirectfbscreen_requestor_la_SOURCES = idirectfbscreen_requestor.c
libidirectfbscreen_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbscreen_requestor_la_LIBADD  = $(LIBS)

libidirectfbsurface_requestor_la_SOURCES = idirectfbsurface_requestor.c idirectfbsurface_requestor.h
libidirectfbsurface_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbsurface_requestor_la_LIBADD  = $(LIBS)

libidirectfbwindow_requestor_la_SOURCES = idirectfbwindow_requestor.c idirectfbwindow_requestor.h
libidirectfbwindow_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbwindow_requestor_la_LIBADD  = $(LIBS)

libifusiondale_requestor_la_SOURCES = ifusiondale_requestor.c
libifusiondale_requestor_la_LDFLAGS = -avoid-version -module
libifusiondale_requestor_la_LIBADD  = $(LIBS)

libicoma_requestor_la_SOURCES = icoma_requestor.c icoma_requestor.h
libicoma_requestor_la_LDFLAGS = -avoid-version -module
libicoma_requestor_la_LIBADD  = $(LIBS)

libicomacomponent_requestor_la_SOURCES = icomacomponent_requestor.c
libicomacomponent_requestor_la_LDFLAGS = -avoid-version -module
libicomacomponent_requestor_la_LIBADD  = $(LIBS)

libifusionsound_requestor_la_SOURCES = ifusionsound_requestor.c
libifusionsound_requestor_la_LDFLAGS = -avoid-version -module
libifusionsound_requestor_la_LIBADD  = $(LIBS)

libifusionsoundstream_requestor_la_SOURCES = ifusionsoundstream_requestor.c
libifusionsoundstream_requestor_la_LDFLAGS = -avoid-version -module
libifusionsoundstream_requestor_la_LIBADD  = $(LIBS) ../compressor/libfusionsound_compressor.la

libifusionsoundbuffer_requestor_la_SOURCES = ifusionsoundbuffer_requestor.c
libifusionsoundbuffer_requestor_la_LDFLAGS = -avoid-version -module
libifusionsoundbuffer_requestor_la_LIBADD  = $(LIBS) ../compressor/libfusionsound_compressor.la

libifusionsoundplayback_requestor_la_SOURCES = ifusionsoundplayback_requestor.c
libifusionsoundplayback_requestor_la_LDFLAGS = -avoid-version -module
libifusionsoundplayback_requestor_la_LIBADD  = $(LIBS)

libidivine_requestor_la_SOURCES = idivine_requestor.c
libidivine_requestor_la_LDFLAGS = -avoid-version -module
libidivine_requestor_la_LIBADD = $(LIBS)

include $(top_srcdir)/rules/libobject.make
