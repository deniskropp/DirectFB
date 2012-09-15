## Makefile.am for DirectFB/interfaces/IDirectFBGL/EGL

idirectfbgldir = $(MODULEDIR)/interfaces/IDirectFBGL

if IDIRECTFBGL_EGL
EGL_LTLIB = libidirectfbgl_egl.la
endif

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_builddir)/src	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src 	\
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

AM_CFLAGS = $(EGL_CFLAGS)

idirectfbgl_LTLIBRARIES = \
	$(EGL_LTLIB)

if BUILD_STATIC
idirectfbgl_DATA = $(idirectfbgl_LTLIBRARIES:.la=.o)
endif


libidirectfbgl_egl_la_SOURCES = idirectfbgl_egl.c

libidirectfbgl_egl_la_LDFLAGS = -avoid-version -module

libidirectfbgl_egl_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

