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

INCLUDES = \
	-I$(top_srcdir)/include			\
	-I$(top_srcdir)/lib			\
	-I$(top_srcdir)/src 			\
	-I$(top_srcdir)/proxy/dispatcher	\
	-DDATADIR=\"@DATADIR@\"

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


libidirectfb_requestor_la_SOURCES = idirectfb_requestor.c
libidirectfb_requestor_la_LDFLAGS = -avoid-version -module
libidirectfb_requestor_la_LIBADD  = $(LIBS)

libidirectfbdatabuffer_requestor_la_SOURCES = idirectfbdatabuffer_requestor.c
libidirectfbdatabuffer_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbdatabuffer_requestor_la_LIBADD  = $(LIBS)

libidirectfbdisplaylayer_requestor_la_SOURCES = idirectfbdisplaylayer_requestor.c
libidirectfbdisplaylayer_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbdisplaylayer_requestor_la_LIBADD  = $(LIBS)

libidirectfbeventbuffer_requestor_la_SOURCES = idirectfbeventbuffer_requestor.c
libidirectfbeventbuffer_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbeventbuffer_requestor_la_LIBADD  = $(LIBS)

libidirectfbfont_requestor_la_SOURCES = idirectfbfont_requestor.c
libidirectfbfont_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbfont_requestor_la_LIBADD  = $(LIBS)

libidirectfbimageprovider_requestor_la_SOURCES = idirectfbimageprovider_requestor.c
libidirectfbimageprovider_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbimageprovider_requestor_la_LIBADD  = $(LIBS)

libidirectfbinputdevice_requestor_la_SOURCES = idirectfbinputdevice_requestor.c
libidirectfbinputdevice_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbinputdevice_requestor_la_LIBADD  = $(LIBS)

libidirectfbpalette_requestor_la_SOURCES = idirectfbpalette_requestor.c
libidirectfbpalette_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbpalette_requestor_la_LIBADD  = $(LIBS)

libidirectfbscreen_requestor_la_SOURCES = idirectfbscreen_requestor.c
libidirectfbscreen_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbscreen_requestor_la_LIBADD  = $(LIBS)

libidirectfbsurface_requestor_la_SOURCES = idirectfbsurface_requestor.c
libidirectfbsurface_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbsurface_requestor_la_LIBADD  = $(LIBS)

libidirectfbwindow_requestor_la_SOURCES = idirectfbwindow_requestor.c
libidirectfbwindow_requestor_la_LDFLAGS = -avoid-version -module
libidirectfbwindow_requestor_la_LIBADD  = $(LIBS)
