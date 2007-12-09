## Makefile.am for DirectFB/src/core/gfxcards/davinci

EXTRA_DIST = \
	directfbrc				\
	Makefile.kernel				\
	kernel-module/c64x/Makefile		\
	kernel-module/c64x/c64x.c		\
	kernel-module/Makefile			\
	kernel-module/include/linux/c64x.h

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_builddir)/lib	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/systems	\
	-I$(srcdir)/kernel-module/include

davinci_LTLIBRARIES = libdirectfb_davinci.la

if BUILD_STATIC
davinci_DATA = $(davinci_LTLIBRARIES:.la=.o)
endif

davincidir = $(MODULEDIR)/gfxdrivers

libdirectfb_davinci_la_SOURCES =	\
	davincifb.h		\
	davinci_2d.c		\
	davinci_2d.h		\
	davinci_c64x.c		\
	davinci_c64x.h		\
	davinci_gfxdriver.c	\
	davinci_gfxdriver.h	\
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
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_davinci_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make
	
kernel-module/c64x/Makefile
kernel-module/c64x/c64x.c
kernel-module/Makefile
kernel-module/include/linux/c64x.h
