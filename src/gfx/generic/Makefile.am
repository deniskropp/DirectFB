## Makefile.am for DirectFB/src/gfx/generic

INCLUDES = \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/src

CFLAGS = $(DFB_CFLAGS) $(DFB_CFLAGS_OMIT_FRAME_POINTER)


if BUILDMMX
ASMSOURCES = \
	Sop_argb_to_Dacc_MMX.s	\
	Sop_rgb16_to_Dacc_MMX.s \
	Sop_rgb32_to_Dacc_MMX.s \
	Sop_argb_Sto_Dacc_MMX.s	\
	Sacc_to_Dop_rgb16_MMX.s	\
	Sacc_to_Dop_rgb32_MMX.s	\
	Cacc_add_to_Dacc_MMX.s	\
	Sacc_add_to_Dacc_MMX.s	\
	Dacc_modulate_argb_MMX.s \
	Xacc_blend_srcalpha_MMX.s \
	Xacc_blend_invsrcalpha_MMX.s
else
ASMSOURCES =
endif


noinst_LTLIBRARIES = libdirectfb_generic.la

libdirectfb_generic_la_SOURCES = \
	generic.c		\
	generic.h		\
	$(ASMSOURCES)
