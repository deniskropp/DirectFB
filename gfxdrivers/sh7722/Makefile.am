## Makefile.am for DirectFB/src/core/gfxcards/sh7722

EXTRA_DIST = \
	directfbrc.sh7722		\
	Makefile.kernel			\
	README.sh7722			\
	kernel-module/Makefile		\
	kernel-module/sh7722gfx.c	\
	kernel-module/sh7722gfx.h

INCLUDES = \
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems		\
	-I$(srcdir)/kernel-module

sh7722_LTLIBRARIES = libdirectfb_sh7722.la

if BUILD_STATIC
sh7722_DATA = $(sh7722_LTLIBRARIES:.la=.o)
endif

sh7722dir = $(MODULEDIR)/gfxdrivers

libdirectfb_sh7722_la_SOURCES =	\
	sh7722.c		\
	sh7722.h		\
	sh7722_blt.c		\
	sh7722_blt.h		\
	sh7722_layer.c		\
	sh7722_layer.h		\
	sh7722_lcd.c		\
	sh7722_lcd.h		\
	sh7722_multi.c		\
	sh7722_multi.h		\
	sh7722_screen.c		\
	sh7722_screen.h		\
	sh7722_types.h

libdirectfb_sh7722_la_LDFLAGS = \
	-export-dynamic		\
	-avoid-version		\
	$(DFB_LDFLAGS)

libdirectfb_sh7722_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

