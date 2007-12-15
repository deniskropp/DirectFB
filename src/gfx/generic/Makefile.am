## Makefile.am for DirectFB/src/gfx/generic

INCLUDES = \
	-I$(top_builddir)/lib		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/include		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src

AM_CFLAGS = $(DFB_CFLAGS_OMIT_FRAME_POINTER)

EXTRA_DIST = \
	yuvtbl-gen.c

internalincludedir = $(INTERNALINCLUDEDIR)/gfx/generic

internalinclude_HEADERS = \
     generic.h

noinst_LTLIBRARIES = libdirectfb_generic.la

if SOFTWARE_RENDERING
GENERIC_C = generic.c
else
GENERIC_C = generic_dummy.c
endif

libdirectfb_generic_la_SOURCES = \
	duffs_device.h			\
	$(GENERIC_C)			\
	generic.h			\
	generic_mmx.h			\
	generic_64.h			\
	stretch_hvx_N.h			\
	stretch_hvx_16.h		\
	stretch_hvx_32.h		\
	stretch_hvx_8.h			\
	stretch_hvx_88.h		\
	stretch_up_down_16.h		\
	stretch_up_down_32.h		\
	stretch_up_down_32_indexed.h	\
	stretch_up_down_8.h		\
	stretch_up_down_88.h		\
	stretch_up_down_table.h		\
	template_acc_16.h		\
	template_acc_32.h		\
	template_colorkey_16.h		\
	template_colorkey_32.h		\
	yuvtbl.h

generic.c: yuvtbl.h
generic_dummy.c: yuvtbl.h

yuvtbl-gen: yuvtbl-gen.c
	$(CC) -O2 yuvtbl-gen.c -o yuvtbl-gen

#yuvtbl.h: yuvtbl-gen
#	./yuvtbl-gen > $(srcdir)/yuvtbl.h

clean-local:
	rm -f yuvtbl-gen

