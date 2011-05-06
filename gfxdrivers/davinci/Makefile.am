## Makefile.am for DirectFB/src/core/gfxcards/davinci

EXTRA_DIST = \
	directfbrc				\
	Makefile.kernel				\
	kernel-module/c64x/Makefile		\
	kernel-module/c64x/c64x.c		\
	kernel-module/Makefile			\
	kernel-module/include/linux/c64x.h	\
	patches/ti-davinci-2.6.10-mvl401-fbio_set_start.patch

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems	\
	-I$(srcdir)/kernel-module/include

bin_PROGRAMS = c64xdump

lib_LTLIBRARIES = libdavinci_c64x.la

davinci_LTLIBRARIES = libdirectfb_davinci.la

if BUILD_STATIC
davinci_DATA = $(davinci_LTLIBRARIES:.la=.o)
endif

davincidir = $(MODULEDIR)/gfxdrivers
includedir = @INCLUDEDIR@
includelinuxdir = @INCLUDEDIR@/linux

libdavinci_c64x_la_SOURCES =	\
	davinci_c64x.c

libdavinci_c64x_la_LIBADD =	\
	$(top_builddir)/lib/direct/libdirect.la

libdirectfb_davinci_la_SOURCES =	\
	davinci_2d.c		\
	davinci_2d.h		\
	davinci_gfxdriver.c	\
	davinci_osd.c		\
	davinci_osd.h		\
	davinci_osd_pool.c	\
	davinci_osd_pool.h	\
	davinci_screen.c	\
	davinci_screen.h	\
	davinci_video.c		\
	davinci_video.h		\
	davinci_video_pool.c	\
	davinci_video_pool.h

libdirectfb_davinci_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_davinci_la_LIBADD = \
	$(top_builddir)/lib/direct/libdirect.la \
	$(top_builddir)/src/libdirectfb.la \
	$(builddir)/libdavinci_c64x.la

c64xdump_SOURCES = c64xdump.c
c64xdump_LDADD   = $(top_builddir)/lib/direct/libdirect.la

include $(top_srcdir)/rules/libobject.make
