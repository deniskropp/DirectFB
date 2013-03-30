libdirect = $(top_builddir)/lib/direct/libdirect.la
libfusion = $(top_builddir)/lib/fusion/libfusion.la

libdirectfb = $(top_builddir)/src/libdirectfb.la

libsawman = $(top_builddir)/lib/sawman/libsawman.la
libdivine = $(top_builddir)/lib/divine/libdivine.la
libfusiondale = $(top_builddir)/lib/fusiondale/libfusiondale.la
libfusionsound = $(top_builddir)/lib/fusionsound/libfusionsound.la


if DIRECTFB_BUILD_VOODOO
libvoodoo = $(top_builddir)/lib/voodoo/libvoodoo.la
else
libvoodoo =
endif

if DIRECTFB_BUILD_ONE
libone = $(top_builddir)/lib/One/libone.la
else
libone =
endif


DFB_BASE_LIBS = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

