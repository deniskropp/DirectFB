## Makefile.am for DirectFB/src

SUBDIRS = core display media windows input misc gfx

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS)

AM_CPPFLAGS = \
	-DDATADIR=\"@DATADIR@\"	\
	-DMODULEDIR=\"@MODULEDIR@\"


internalincludedir = $(INTERNALINCLUDEDIR)

internalinclude_HEADERS = idirectfb.h


lib_LTLIBRARIES = libdirectfb.la

libdirectfb_la_SOURCES = 	\
	directfb.c		\
	idirectfb.c		\
	interface.c

libdirectfb_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-release $(LT_RELEASE)					\
	-export-dynamic						\
	-export-symbols $(srcdir)/export_list.sym		\
	$(DFB_LDFLAGS)

libdirectfb_la_LIBADD = \
	display/libdirectfb_display.la	\
	media/libdirectfb_media.la	\
	windows/libdirectfb_windows.la	\
	input/libdirectfb_input.la	\
	misc/libdirectfb_misc.la	\
	gfx/libdirectfb_gfx.la		\
	core/libdirectfb_core.la

EXTRA_DIST = export_list.sym
