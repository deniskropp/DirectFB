## Makefile.am for DirectFB/tests

if DIRECTFB_BUILD_VOODOO
SUBDIRS = voodoo

VOODOO_PROGS = \
	voodoo_bench	\
	voodoo_bench_client		\
	voodoo_bench_server		\
	voodoo_bench_client_unix	\
	voodoo_bench_server_unix
endif


INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-DLINUX_2_6

if GFX_GLES2
GL_PROGS = \
	dfbtest_gl1	\
	dfbtest_gl2	\
	dfbtest_gl3
else
GL_PROGS =
endif

if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_PROGS = 
else
NON_PURE_VOODOO_PROGS = \
	coretest_blit2	\
	fusion_call	\
	fusion_call_bench	\
	fusion_fork	\
	fusion_reactor	\
	fusion_skirmish	\
	fusion_stream
endif

bin_PROGRAMS = \
	$(GL_PROGS)	\
	$(NON_PURE_VOODOO_PROGS)	\
	dfbtest_blit	\
	dfbtest_blit2	\
	dfbtest_fillrect	\
	dfbtest_font	\
	dfbtest_mirror	\
	dfbtest_reinit	\
	dfbtest_scale	\
	dfbtest_sync	\
	dfbtest_water	\
	dfbtest_window	\
	dfbtest_window_flip_once	\
	direct_stream	\
	direct_test	\
	$(VOODOO_PROGS)

libdirectfb = $(top_builddir)/src/libdirectfb.la
libfusion = $(top_builddir)/lib/fusion/libfusion.la
libdirect = $(top_builddir)/lib/direct/libdirect.la

if DIRECTFB_BUILD_VOODOO
libvoodoo = \
        ../lib/voodoo/libvoodoo.la
else
libvoodoo =
endif

coretest_blit2_SOURCES = coretest_blit2.c
coretest_blit2_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_blit_SOURCES = dfbtest_blit.c
dfbtest_blit_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_blit2_SOURCES = dfbtest_blit2.c
dfbtest_blit2_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_fillrect_SOURCES = dfbtest_fillrect.c
dfbtest_fillrect_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_font_SOURCES = dfbtest_font.c
dfbtest_font_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_gl1_SOURCES = dfbtest_gl1.c
dfbtest_gl1_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_gl1_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect) $(GLES2_LIBS)

dfbtest_gl2_SOURCES = dfbtest_gl2.c
dfbtest_gl2_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_gl2_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect) $(GLES2_LIBS)

dfbtest_gl3_SOURCES = dfbtest_gl3.c
dfbtest_gl3_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_gl3_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect) $(GLES2_LIBS)

dfbtest_mirror_SOURCES = dfbtest_mirror.c
dfbtest_mirror_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_reinit_SOURCES = dfbtest_reinit.c
dfbtest_reinit_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_scale_SOURCES = dfbtest_scale.c
dfbtest_scale_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_sync_SOURCES = dfbtest_sync.c
dfbtest_sync_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_water_SOURCES = dfbtest_water.c
dfbtest_water_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_SOURCES = dfbtest_window.c
dfbtest_window_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_flip_once_SOURCES = dfbtest_window_flip_once.c
dfbtest_window_flip_once_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)


direct_stream_SOURCES = direct_stream.c
direct_stream_LDADD   = $(libdirect)

direct_test_SOURCES = direct_test.c
direct_test_LDADD   = $(libdirect)


fusion_call_SOURCES = fusion_call.c
fusion_call_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

fusion_call_bench_SOURCES = fusion_call_bench.c
fusion_call_bench_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

fusion_fork_SOURCES = fusion_fork.c
fusion_fork_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

fusion_reactor_SOURCES = fusion_reactor.c
fusion_reactor_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

fusion_skirmish_SOURCES = fusion_skirmish.c
fusion_skirmish_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

fusion_stream_SOURCES = fusion_stream.c
fusion_stream_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_SOURCES = voodoo_bench.c
voodoo_bench_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_client_SOURCES = voodoo_bench_client.c
voodoo_bench_client_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_server_SOURCES = voodoo_bench_server.c
voodoo_bench_server_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_client_unix_SOURCES = voodoo_bench_client_unix.c
voodoo_bench_client_unix_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_server_unix_SOURCES = voodoo_bench_server_unix.c
voodoo_bench_server_unix_LDADD   = $(libdirectfb) $(libvoodoo) $(libfusion) $(libdirect)
