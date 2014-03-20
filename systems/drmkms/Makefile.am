## Makefile.am for DirectFB/systems/drmkms

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/lib/egl		\
	$(DRMKMS_CFLAGS)		\
	$(GBM_CFLAGS)


internalincludedir = $(INTERNALINCLUDEDIR)/drmkms_system

internalinclude_HEADERS = \
	drmkms_system.h


systemsdir = $(MODULEDIR)/systems

if BUILD_STATIC
systems_DATA = libdirectfb_drmkms_system.o
endif
systems_LTLIBRARIES = libdirectfb_drmkms_system.la

libdirectfb_drmkms_system_la_LDFLAGS = \
	-avoid-version	\
	-module \
	$(DRMKMS_LIBS)	\
	$(GBM_LIBS)

libdirectfb_drmkms_system_la_SOURCES = \
	drmkms_layer.c		\
	drmkms_screen.c		\
	drmkms_system.c		\
	drmkms_surface_pool.c     \
	vt.c			\
	vt.h

libdirectfb_drmkms_system_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/lib/fusion/libfusion.la \
	$(top_builddir)/src/libdirectfb.la



if EGL_UNITED


graphicsdir = $(MODULEDIR)/graphics

if BUILD_STATIC
graphics_DATA = libdfbdrm_egl.o
endif

graphics_LTLIBRARIES = libdfbdrm_egl.la


libdfbdrm_egl_la_LIBADD = \
	$(DFB_BASE_LIBS)	\
	$(GLES2_DFB_BASE_LIBS)	\
	$(libdfbegl)

libdfbdrm_egl_la_SOURCES = \
	DRMEGLImpl.cpp		\
	DRMEGLImpl.h

libdfbdrm_egl_la_LDFLAGS = \
	$(DRMKMS_LIBS)	\
	$(GBM_LIBS)	\
	-avoid-version	\
	-module


endif	# EGL_UNITED



include $(top_srcdir)/rules/libobject.make
include $(top_srcdir)/rules/libs_deps.make
