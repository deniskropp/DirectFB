## Makefile.am for DirectFB/tests

INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-DLINUX_2_6


noinst_PROGRAMS = \
	dfbtest_blit	\
	dfbtest_reinit	\
	dfbtest_scale	\
	dfbtest_window	\
	direct_stream	\
	direct_test	\
	fusion_fork	\
	fusion_reactor	\
	fusion_skirmish	\
	fusion_stream

libdirectfb = $(top_builddir)/src/libdirectfb.la
libfusion = $(top_builddir)/lib/fusion/libfusion.la
libdirect = $(top_builddir)/lib/direct/libdirect.la

dfbtest_blit_SOURCES = dfbtest_blit.c
dfbtest_blit_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_reinit_SOURCES = dfbtest_reinit.c
dfbtest_reinit_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_scale_SOURCES = dfbtest_scale.c
dfbtest_scale_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

dfbtest_window_SOURCES = dfbtest_window.c
dfbtest_window_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)


direct_stream_SOURCES = direct_stream.c
direct_stream_LDADD   = $(libdirect)

direct_test_SOURCES = direct_test.c
direct_test_LDADD   = $(libdirect)


fusion_fork_SOURCES = fusion_fork.c
fusion_fork_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_reactor_SOURCES = fusion_reactor.c
fusion_reactor_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_skirmish_SOURCES = fusion_skirmish.c
fusion_skirmish_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)

fusion_stream_SOURCES = fusion_stream.c
fusion_stream_LDADD   = $(libdirectfb) $(libfusion) $(libdirect)
