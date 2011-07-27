## Makefile.am for DirectFB/src/core/gfxcards/sh7722

EXTRA_DIST = \
	directfbrc.sh7722		\
	directfbrc.sh7723		\
	Makefile.kernel			\
	README.sh7722			\
	kernel-module/sh772x_driver.c	\
	kernel-module/sh772x_gfx.h	\
	kernel-module/sh7722.c		\
	kernel-module/sh7722.h		\
	kernel-module/sh7723.c		\
	kernel-module/sh7723.h		\
	kernel-module/Makefile

INCLUDES = \
	-I$(top_builddir)/include	\
	-I$(top_builddir)/lib		\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/systems		\
	-I$(srcdir)/kernel-module


sh7722_LTLIBRARIES = libdirectfb_sh7722.la

if BUILD_STATIC
sh7722_DATA = $(sh7722_LTLIBRARIES:.la=.o)
endif

sh7722dir = $(MODULEDIR)/gfxdrivers

if SH772X_SHJPEG
SH772X_JPEG_SOURCE = sh7722_jpeg.c
endif

libdirectfb_sh7722_la_SOURCES =	\
	sh7722.c		\
	sh7722.h		\
	sh7722_blt.c		\
	sh7722_blt.h		\
	sh7723_blt.c		\
	sh7723_blt.h		\
	sh7722_layer.c		\
	sh7722_layer.h		\
	sh7722_lcd.c		\
	sh7722_lcd.h		\
	sh7722_multi.c		\
	sh7722_multi.h		\
	sh7722_regs.h		\
	sh7722_screen.c		\
	sh7722_screen.h		\
	sh7722_types.h		\
	${SH772X_JPEG_SOURCE}

libdirectfb_sh7722_la_CFLAGS = $(SH772X_DEP_CFLAGS)

libdirectfb_sh7722_la_LDFLAGS = \
	-module			\
	-avoid-version		\
	$(DFB_LDFLAGS)		\
	$(SH772X_DEP_LIBS)	\
	${SH772X_SHJPEG_DEP_LIBS}

libdirectfb_sh7722_la_LIBADD = \
	$(top_builddir)/src/libdirectfb.la


include $(top_srcdir)/rules/libobject.make

