## Makefile.am for DirectFB/tests

AM_CPPFLAGS = \
	-DDATADIR=\"${RUNTIME_SYSROOT}@DATADIR@\"

if DIRECTFB_BUILD_ONE
ONE_PROGS = \
	OneBench	\
	OneTest
endif


if DIRECTFB_BUILD_VOODOO
SUBDIRS = voodoo

VOODOO_PROGS = \
	voodoo_bench	\
	voodoo_bench_client		\
	voodoo_bench_server		\
	voodoo_bench_client_unix	\
	voodoo_bench_server_unix
endif

if ENABLE_SAWMAN
SAWMAN_PROGS = \
	sample1	\
	testrun	\
	testman
endif

if ENABLE_DIVINE
DIVINE_PROGS = \
	divine_test
endif

if ENABLE_FUSIONDALE
FUSIONDALE_PROGS = \
	fdtest_bench \
	fdtest_coma \
	fdtest_init
endif



INCLUDES = \
	-I$(top_builddir)/include \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/lib/One \
	-I$(top_builddir)/lib/divine \
	-I$(top_builddir)/lib/fusiondale \
	-I$(top_builddir)/lib/sawman \
	-I$(top_builddir)/src	\
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/lib/One	\
	-I$(top_srcdir)/lib/divine	\
	-I$(top_srcdir)/lib/fusiondale	\
	-I$(top_srcdir)/lib/sawman	\
	-I$(top_srcdir)/src	\
	-DLINUX_2_6

GL_PROGS_ = \
	dfbtest_gl1	\
	dfbtest_gl2	\
	dfbtest_gl3	\
	dfbtest_old_gl2

if GFX_GL
GL_PROGS = $(GL_PROGS_)
endif

if GFX_GLES2
GL_PROGS = $(GL_PROGS_)
endif

if DIRECTFB_BUILD_PURE_VOODOO
NON_PURE_VOODOO_PROGS = 
else
NON_PURE_VOODOO_PROGS = \
	coretest_blit2	\
	coretest_task	\
	coretest_task_fillrect	\
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
	dfbtest_blit_multi	\
	dfbtest_blit_threads	\
	dfbtest_blit2	\
	dfbtest_clipboard	\
	dfbtest_fillrect	\
	dfbtest_flip	\
	dfbtest_font	\
	dfbtest_init	\
	dfbtest_input	\
	dfbtest_layers \
	dfbtest_mirror	\
	dfbtest_prealloc	\
	dfbtest_reinit	\
	dfbtest_resize	\
	dfbtest_scale	\
	dfbtest_scale_nv21	\
	dfbtest_stereo_window	\
	dfbtest_surface_compositor	\
	dfbtest_surface_compositor_threads	\
	dfbtest_surface_updates	\
	dfbtest_sync	\
	dfbtest_video	\
	dfbtest_waitserial	\
	dfbtest_water	\
	dfbtest_window	\
	dfbtest_window_cursor	\
	dfbtest_window_flip	\
	dfbtest_window_flip_once	\
	dfbtest_window_surface	\
	dfbtest_windows_watcher	\
	direct_stream	\
	direct_test	\
	$(ONE_PROGS)	\
	$(VOODOO_PROGS) \
	$(SAWMAN_PROGS) \
	$(DIVINE_PROGS) \
	$(FUSIONDALE_PROGS)

libdirectfb = $(top_builddir)/src/libdirectfb.la
libfusion = $(top_builddir)/lib/fusion/libfusion.la
libdirect = $(top_builddir)/lib/direct/libdirect.la
libsawman = $(top_builddir)/lib/sawman/libsawman.la
libdivine = $(top_builddir)/lib/divine/libdivine.la
libfusiondale = $(top_builddir)/lib/fusiondale/libfusiondale.la

if DIRECTFB_BUILD_VOODOO
libvoodoo = \
        ../lib/voodoo/libvoodoo.la
else
libvoodoo =
endif

if DIRECTFB_BUILD_ONE
libone = \
        ../lib/One/libone.la
else
libone =
endif

coretest_blit2_SOURCES = coretest_blit2.c
coretest_blit2_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

coretest_task_SOURCES = coretest_task.cpp
coretest_task_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

coretest_task_fillrect_SOURCES = coretest_task_fillrect.cpp
coretest_task_fillrect_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_blit_SOURCES = dfbtest_blit.c
dfbtest_blit_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_blit_multi_SOURCES = dfbtest_blit_multi.c
dfbtest_blit_multi_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_blit_threads_SOURCES = dfbtest_blit_threads.c
dfbtest_blit_threads_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_blit2_SOURCES = dfbtest_blit2.c
dfbtest_blit2_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_clipboard_SOURCES = dfbtest_clipboard.c
dfbtest_clipboard_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_fillrect_SOURCES = dfbtest_fillrect.c
dfbtest_fillrect_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_flip_SOURCES = dfbtest_flip.c
dfbtest_flip_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_font_SOURCES = dfbtest_font.c
dfbtest_font_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_init_SOURCES = dfbtest_init.c
dfbtest_init_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_input_SOURCES = dfbtest_input.c
dfbtest_input_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_layers_SOURCES = dfbtest_layers.c
dfbtest_layers_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_gl1_SOURCES = dfbtest_gl1.c
dfbtest_gl1_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_gl1_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(GL_LIBS) $(GLES2_LIBS)

dfbtest_gl2_SOURCES = dfbtest_gl2.c
dfbtest_gl2_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_gl2_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(GL_LIBS) $(GLES2_LIBS)

dfbtest_gl3_SOURCES = dfbtest_gl3.c
dfbtest_gl3_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_gl3_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(GL_LIBS) $(GLES2_LIBS)

dfbtest_old_gl2_SOURCES = dfbtest_old_gl2.c
dfbtest_old_gl2_CFLAGS  = $(GLES2_CFLAGS)
dfbtest_old_gl2_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(GL_LIBS) $(GLES2_LIBS)

dfbtest_mirror_SOURCES = dfbtest_mirror.c
dfbtest_mirror_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_prealloc_SOURCES = dfbtest_prealloc.c
dfbtest_prealloc_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_reinit_SOURCES = dfbtest_reinit.c
dfbtest_reinit_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_resize_SOURCES = dfbtest_resize.c
dfbtest_resize_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_scale_SOURCES = dfbtest_scale.c
dfbtest_scale_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_scale_nv21_SOURCES = dfbtest_scale_nv21.c
dfbtest_scale_nv21_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_stereo_window_SOURCES = dfbtest_stereo_window.c
dfbtest_stereo_window_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_surface_compositor_SOURCES = dfbtest_surface_compositor.c
dfbtest_surface_compositor_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_surface_compositor_threads_SOURCES = dfbtest_surface_compositor_threads.c
dfbtest_surface_compositor_threads_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_surface_updates_SOURCES = dfbtest_surface_updates.c
dfbtest_surface_updates_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_sync_SOURCES = dfbtest_sync.c
dfbtest_sync_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_video_SOURCES = dfbtest_video.c
dfbtest_video_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_waitserial_SOURCES = dfbtest_waitserial.c
dfbtest_waitserial_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_water_SOURCES = dfbtest_water.c
dfbtest_water_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_SOURCES = dfbtest_window.c
dfbtest_window_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_cursor_SOURCES = dfbtest_window_cursor.c
dfbtest_window_cursor_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_flip_SOURCES = dfbtest_window_flip.c
dfbtest_window_flip_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_flip_once_SOURCES = dfbtest_window_flip_once.c
dfbtest_window_flip_once_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_window_surface_SOURCES = dfbtest_window_surface.c
dfbtest_window_surface_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

dfbtest_windows_watcher_SOURCES = dfbtest_windows_watcher.c
dfbtest_windows_watcher_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)


direct_stream_SOURCES = direct_stream.c
direct_stream_LDADD   = $(libdirect)

direct_test_SOURCES = direct_test.c
direct_test_LDADD   = $(libdirect)


fusion_call_SOURCES = fusion_call.c
fusion_call_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

fusion_call_bench_SOURCES = fusion_call_bench.c
fusion_call_bench_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

fusion_fork_SOURCES = fusion_fork.c
fusion_fork_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

fusion_reactor_SOURCES = fusion_reactor.c
fusion_reactor_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

fusion_skirmish_SOURCES = fusion_skirmish.c
fusion_skirmish_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

fusion_stream_SOURCES = fusion_stream.c
fusion_stream_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

OneBench_SOURCES = OneBench.c
OneBench_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

OneTest_SOURCES = OneTest.c
OneTest_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_SOURCES = voodoo_bench.c
voodoo_bench_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_client_SOURCES = voodoo_bench_client.c
voodoo_bench_client_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_server_SOURCES = voodoo_bench_server.c
voodoo_bench_server_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_client_unix_SOURCES = voodoo_bench_client_unix.c
voodoo_bench_client_unix_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

voodoo_bench_server_unix_SOURCES = voodoo_bench_server_unix.c
voodoo_bench_server_unix_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect)

testman_SOURCES = testman.c
testman_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(libsawman)

testrun_SOURCES = testrun.c
testrun_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(libsawman)

sample1_SOURCES = sample1.c
sample1_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(libsawman)

divine_test_SOURCES = divine_test.c
divine_test_LDADD   = $(libdirectfb) $(libone) $(libvoodoo) $(libfusion) $(libdirect) $(libdivine)

fdtest_bench_SOURCES = fdtest_bench.c
fdtest_bench_LDADD   = $(libfusion) $(libdirect) $(libfusiondale)

fdtest_coma_SOURCES = fdtest_coma.c
fdtest_coma_LDADD   = $(libfusion) $(libdirect) $(libfusiondale)

fdtest_init_SOURCES = fdtest_init.c
fdtest_init_LDADD   = $(libfusion) $(libdirect) $(libfusiondale)
