## Makefile.am for DirectFB/tests

INCLUDES = \
	-I$(top_builddir)/lib \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/include	\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-DLINUX_2_6


noinst_PROGRAMS = \
	direct_stream	\
	direct_test	\
	fusion_fork	\
	fusion_reactor	\
	fusion_skirmish	\
	fusion_stream


direct_stream_SOURCES = direct_stream.c
direct_stream_LDADD   = ../lib/direct/libdirect.la

direct_test_SOURCES = direct_test.c
direct_test_LDADD   = ../lib/direct/libdirect.la


fusion_fork_SOURCES = fusion_fork.c
fusion_fork_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la

fusion_reactor_SOURCES = fusion_reactor.c
fusion_reactor_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la

fusion_skirmish_SOURCES = fusion_skirmish.c
fusion_skirmish_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la

fusion_stream_SOURCES = fusion_stream.c
fusion_stream_LDADD   = ../src/libdirectfb.la ../lib/fusion/libfusion.la ../lib/direct/libdirect.la
