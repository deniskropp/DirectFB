## Makefile.am for DirectFB/interfaces/IDirectFBWindows

idirectfbwindowsdir = $(MODULEDIR)/interfaces/IDirectFBWindows


INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src

AM_CPPFLAGS = -DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

if ENABLE_VOODOO
idirectfbwindows_VOODOOLIBRARIES = \
	libidirectfbwindows_dispatcher.la	\
	libidirectfbwindows_requestor.la
endif

idirectfbwindows_LTLIBRARIES = \
	libidirectfbwindows_default.la		\
	$(idirectfbwindows_VOODOOLIBRARIES)

if BUILD_STATIC
idirectfbwindows_DATA = $(idirectfbwindows_LTLIBRARIES:.la=.o)
endif


libidirectfbwindows_default_la_SOURCES = \
	idirectfbwindows_default.c		\
	idirectfbwindows_default.h

libidirectfbwindows_dispatcher_la_SOURCES = \
	idirectfbwindows_dispatcher.c		\
	idirectfbwindows_dispatcher.h

libidirectfbwindows_requestor_la_SOURCES = \
	idirectfbwindows_requestor.c		\
	idirectfbwindows_requestor.h

libidirectfbwindows_default_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbwindows_dispatcher_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/voodoo/libvoodoo.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbwindows_requestor_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/voodoo/libvoodoo.la \
	$(top_builddir)/src/libdirectfb.la

libidirectfbwindows_default_la_LDFLAGS = -avoid-version -module

libidirectfbwindows_dispatcher_la_LDFLAGS = -avoid-version -module

libidirectfbwindows_requestor_la_LDFLAGS = -avoid-version -module


include $(top_srcdir)/rules/libobject.make
