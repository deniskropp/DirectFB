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

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"@DATADIR@\"

idirectfb_LTLIBRARIES = \
	libidirectfb_dispatcher.la

idirectfbdatabuffer_LTLIBRARIES = \
	libidirectfbdatabuffer_dispatcher.la

idirectfbeventbuffer_LTLIBRARIES = \
	libidirectfbeventbuffer_dispatcher.la

idirectfbfont_LTLIBRARIES = \
	libidirectfbfont_dispatcher.la

idirectfbimageprovider_LTLIBRARIES = \
	libidirectfbimageprovider_dispatcher.la

idirectfbpalette_LTLIBRARIES = \
	libidirectfbpalette_dispatcher.la

idirectfbscreen_LTLIBRARIES = \
	libidirectfbscreen_dispatcher.la

idirectfbsurface_LTLIBRARIES = \
	libidirectfbsurface_dispatcher.la


libidirectfb_dispatcher_la_SOURCES = idirectfb_dispatcher.c
libidirectfb_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbdatabuffer_dispatcher_la_SOURCES = idirectfbdatabuffer_dispatcher.c
libidirectfbdatabuffer_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbeventbuffer_dispatcher_la_SOURCES = idirectfbeventbuffer_dispatcher.c
libidirectfbeventbuffer_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbfont_dispatcher_la_SOURCES = idirectfbfont_dispatcher.c
libidirectfbfont_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbimageprovider_dispatcher_la_SOURCES = idirectfbimageprovider_dispatcher.c
libidirectfbimageprovider_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbpalette_dispatcher_la_SOURCES = idirectfbpalette_dispatcher.c
libidirectfbpalette_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbscreen_dispatcher_la_SOURCES = idirectfbscreen_dispatcher.c
libidirectfbscreen_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbsurface_dispatcher_la_SOURCES = idirectfbsurface_dispatcher.c
libidirectfbsurface_dispatcher_la_LDFLAGS = -avoid-version -module
