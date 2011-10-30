## Makefile.am for DirectFB/tests

if DIRECTFB_BUILD_VOODOO
SUBDIRS = voodoo
endif


INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-DLINUX_2_6

if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_PROGS = 
else
NON_PURE_VOODOO_PROGS = \
	coretest_blit2	\
	fusion_call	\
	fusion_fork	\
	fusion_reactor	\
	fusion_skirmish	\
	fusion_stream
endif

bin_PROGRAMS = \
	$(NON_PURE_VOODOO_PROGS)	\
	dfbtest_blit	\
	dfbtest_blit2	\
	dfbtest_fillrect	\
	dfbtest_font	\
	dfbtest_mirror	\
	dfbtest_prealloc	\
	dfbtest_reinit	\
	dfbtest_resize	\
	dfbtest_scale	\
	dfbtest_sync	\
	dfbtest_window	\
	dfbtest_window_flip_once	\
	direct_stream	\
	direct_test

libdirectfb = $(top_builddir)/src/libdirectfb.la
libfusion = $(top_builddir)/lib/fusion/libfusion.la
libdirect = $(top_builddir)/lib/direct/libdirect.la

coretest_blit2_SOURCES = coretest_blit2.c
coretest_blit2_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_blit_SOURCES = dfbtest_blit.c
dfbtest_blit_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_blit2_SOURCES = dfbtest_blit2.c
dfbtest_blit2_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_fillrect_SOURCES = dfbtest_fillrect.c
dfbtest_fillrect_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_font_SOURCES = dfbtest_font.c
dfbtest_font_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_mirror_SOURCES = dfbtest_mirror.c
dfbtest_mirror_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_prealloc_SOURCES = dfbtest_prealloc.c
dfbtest_prealloc_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_reinit_SOURCES = dfbtest_reinit.c
dfbtest_reinit_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_resize_SOURCES = dfbtest_resize.c
dfbtest_resize_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_scale_SOURCES = dfbtest_scale.c
dfbtest_scale_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_sync_SOURCES = dfbtest_sync.c
dfbtest_sync_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_window_SOURCES = dfbtest_window.c
dfbtest_window_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_window_flip_once_SOURCES = dfbtest_window_flip_once.c
dfbtest_window_flip_once_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)


direct_stream_SOURCES = direct_stream.c
direct_stream_LDADD   = $(libdirect)

direct_test_SOURCES = direct_test.c
direct_test_LDADD   = $(libdirect)


fusion_call_SOURCES = fusion_call.c
fusion_call_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_fork_SOURCES = fusion_fork.c
fusion_fork_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_reactor_SOURCES = fusion_reactor.c
fusion_reactor_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_skirmish_SOURCES = fusion_skirmish.c
fusion_skirmish_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_stream_SOURCES = fusion_stream.c
fusion_stream_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)
